#ifndef _TCPCLIENT_
#define _TCPCLIENT_

#include <atomic>
#include <mutex>
#include <semaphore>

#include "connector.hpp"
#include "eventLoop.hpp"
#include "tcpConnection.hpp"
namespace gdrpc {
namespace net {

class Connector;
class TcpClient : noncopyable {
 public:
  TcpClient(EventLoop* loop, const InetAddress& serverAddr,
            const std::string& nameArg);
  ~TcpClient();

  void connect();
  void disconnect();
  void stop();
  void already();

  TcpConnectionPtr connection() {
    std::unique_lock lock(mutex_);
    return connection_;
  }

  EventLoop* getLoop() const { return loop_; }
  bool retry() const { return retry_; }
  void enableRetry() { retry_ = true; }

  const std::string& name() const { return name_; }

  // 异步的发送 message
  // 如果发送 message 成功，会调用 done 函数， 函数的入参就是 message 对象
  // void writeMessage(AbstractProtocol::s_ptr message,
  // std::function<void(AbstractProtocol::s_ptr)> done);

  // // 异步的读取 message
  // // 如果读取 message 成功，会调用 done 函数， 函数的入参就是 message 对象
  // void readMessage(const std::string& msg_id,
  //                  std::function<void(AbstractProtocol::s_ptr)> done);

  // 线程不安全，但想不到什么时候会多线程同时设置回调
  void setConnectionCallback(ConnectionCallback cb) {
    connectionCallback_ = std::move(cb);
  }

  void setMessageCallback(MessageCallback cb) {
    messageCallback_ = std::move(cb);
  }

  void setWriteCompleteCallback(WriteCompleteCallback cb) {
    writeCompleteCallback_ = std::move(cb);
  }

 private:
  using ConnectorPtr = std::shared_ptr<Connector>;

  void newConnection(std::unique_ptr<Channel>&&);
  void removeConnection(const TcpConnectionPtr& conn);

  EventLoop* loop_;
  ConnectorPtr connector_;
  const std::string name_;
  ConnectionCallback connectionCallback_;
  MessageCallback messageCallback_;
  WriteCompleteCallback writeCompleteCallback_;
  std::atomic_bool retry_;
  std::atomic_bool connect_;  // TODO:改成位状态加锁，阻止重复改变状态
  // always in loop thread
  int nextConnId_;
  std::mutex mutex_;
  std::binary_semaphore latch_;
  TcpConnectionPtr connection_;
};
using TcpClientPtr = std::shared_ptr<TcpClient>;

}  // namespace net
}  // namespace gdrpc

#endif
