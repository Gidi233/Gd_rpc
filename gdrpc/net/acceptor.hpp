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