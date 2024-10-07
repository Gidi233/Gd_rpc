#ifndef _ACCEPTOR_
#define _ACCEPTOR_
#include <functional>

#include "../util/noncopyable.hpp"
#include "channel.hpp"
#include "socket.hpp"

namespace gdrpc {
namespace net {
class EventLoop;
class InetAddress;
// 可以有多个acceptor监听同一个listenfd 操作系统会将取出连接的操作串行化
// 避免并发问题
// TODO: 通过给listenfd添加 SO_REUSEADDR SO_REUSEPORT参数
// 可以为每个eventloop创建一个listenfd（不好搞负载均衡
// 搞个连接数与挂起事件占比加权的优先队列）
class Acceptor : noncopyable {
 public:
  using NewConnectionCallback =
      std::function<void(int sockfd, const InetAddress&)>;

  Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
  ~Acceptor();

  void setNewConnectionCallback(const NewConnectionCallback& cb) {
    NewConnectionCallback_ = cb;
  }

  bool listenning() const { return listenning_; }
  void listen();

 private:
  void handleRead();
  // Acceptor用的就是用户定义的那个baseLoop,在多线程，该线程只负责接收新连接
  EventLoop* loop_;
  Socket acceptSocket_;
  Channel acceptChannel_;
  NewConnectionCallback NewConnectionCallback_;
  bool listenning_;
};
}  // namespace net
}  // namespace gdrpc
#endif