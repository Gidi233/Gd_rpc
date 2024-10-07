#ifndef _CLUSTERCLI_
#define _CLUSTERCLI_
#include <memory>
#include <ranges>
#include <string_view>

#include "../net/callback.hpp"
#include "../net/eventLoop.hpp"
#include "../net/eventLoop_thread.hpp"
#include "../net/tcpClient.hpp"
#include "../net/tcpConnection.hpp"
#include "../rpc/zkClient.hpp"
#include "../util/log.hpp"
#include "../util/timestamp.hpp"
#include "./rpcChannelCli.hpp"
namespace gdrpc::rpc {
template <typename T>
class Client {
 public:
  Client(int weight, std::shared_ptr<T> client)
      : weight_(weight), client_(client) {}

  int weight_;
  std::shared_ptr<T> client_;
};
template <typename T>
using ClientPtr = std::shared_ptr<Client<T>>;

template <typename T>
concept LBType = requires(T t, const typename T::value_type v) {
  { t.Add(v) };  // 检查 Add 方法
  { t.Next() } -> std::same_as<const typename T::value_type>;  // 检查 Next 方法
};
template <typename T, LBType LB>
class RpcCluster {
 public:
  RpcCluster(net::EventLoop* loop, ZkClientPtr zkcli, std::string path)
      : loop_(loop), zkCli_(zkcli) {
    auto str = zkCli_->GetData(path.c_str());
    auto endpoints =
        str | std::views::split('\n') | std::views::transform([](auto&& range) {
          return std::string_view(range.begin(), range.end());
        });
    for (const auto& endpoint : endpoints) {
      std::vector<std::string> tmp;
      std::stringstream ss(endpoint);
      std::string t;

      while (std::getline(ss, t, ' ')) {
        tmp.push_back(t);
      }
      std::string key = tmp[0] + ":" + tmp[1];
      net::InetAddress address(atoi(tmp[0].c_str()), tmp[1]);
      ClientPtr<T> cli = make_shared(tmp[2], make_shared<T>(loop_, address));
      loadBalence_.add(cli);
      clientMap_[key] = cli;
    }
  }
  template <typename FN>
  auto call(FN fn) {
    auto clientPtr = loadBalence_.Next();
    if (clientPtr != nullptr) return fn(loadBalence_.Next()->client_.get());
  }

 private:
  net::EventLoop* loop_;
  ZkClientPtr zkCli_;
  std::unordered_map<std::string, ClientPtr<T>> clientMap_;

  LB loadBalence_;
};
template <typename T>
class RoundRobin {
 public:
  void Add(const T t);
  T Next();

 private:
  std::list<T> elements_;
};

template <typename T>
void RoundRobin<T>::Add(const T t) {
  elements_.push_back(t);
}

template <typename T>
T RoundRobin<T>::Next() {
  if (elements_.empty()) return nullptr;
  if (elements_.size() == 1) return elements_.front().client_;

  int total = 0;
  T* p = &elements_[0];
  for (size_t i = 0; i < elements_.size(); ++i) {
    total += elements_[i]->weight_;
    elements_[i]->currentWeight_ += elements_[i]->weight_;
    if (elements_[i]->currentWeight_ > p->currentWeight_) {
      p = &elements_[i];
    }
  }
  p->currentWeight_ -= total;
  return *p;
}
}  // namespace gdrpc::rpc
#endif