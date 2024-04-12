#ifndef _CONNECTOR_
#define _CONNECTOR_

#include <atomic>
#include <functional>
#include <memory>

#include "../util/noncopyable.hpp"
#include "inet_address.hpp"

namespace gdrpc ::net {
class Channel;
class EventLoop;
class InetAddress;
// 封装了客户端异步connect的过程
class Connector : noncopyable, public std::enable_shared_from_this<Connector> {
 public:
  using NewConnectionCallback = std::function<void(std::unique_ptr<Channel>&&)>;

  Connector(EventLoop* loop, const InetAddress& serverAddr);
  ~Connector();

  void setNewConnectionCallback(const NewConnectionCallback& cb) {
    newConnectionCallback_ = cb;
  }

  void start();    // can be called in any thread
  void restart();  // must be called in loop thread
  void stop();     // can be called in any thread

  const InetAddress& serverAddress() const { return serverAddr_; }

 private:
  enum States { kDisconnected, kConnecting, kConnected };
  static const int kMaxRetryDelayMs = 30 * 1000;
  static const int kInitRetryDelayMs = 500;

  void setState(States s) { state_ = s; }
  void startInLoop();
  void stopInLoop();
  void connect();
  void connecting(int sockfd);
  void handleWrite();
  void handleError();
  void retry(int sockfd);
  int removeAndResetChannel();
  void resetChannel();

  EventLoop* loop_;
  InetAddress serverAddr_;
  std::atomic_bool connect_;
  std::atomic<States> state_;
  std::unique_ptr<Channel> channel_;
  NewConnectionCallback newConnectionCallback_;
  int retryDelayMs_;
};

}  // namespace gdrpc::net

#endif
