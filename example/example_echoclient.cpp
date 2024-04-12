#include <unistd.h>

#include <iostream>
#include <utility>

#include "../gdrpc/net/callback.hpp"
#include "../gdrpc/net/eventLoop.hpp"
#include "../gdrpc/net/eventLoop_thread.hpp"
#include "../gdrpc/net/tcpClient.hpp"
#include "../gdrpc/net/tcpConnection.hpp"
#include "../gdrpc/util/async_log.hpp"
#include "../gdrpc/util/log.hpp"
#include "../gdrpc/util/timestamp.hpp"

using namespace gdrpc;
using namespace gdrpc::net;
using namespace gdrpc::gdlog;

#define LENGTH 10
#define TIME 10000
#define kRollSize (20 * 1024 * 1024)

std::unique_ptr<gdrpc::gdlog::AsyncLogging> g_asyncLog;
std::condition_variable cond;
std::mutex mutex;
std::string all;
class EchoClient {
 public:
  EchoClient(EventLoop* loop, const InetAddress& listenAddr)
      : loop_(loop), client_(loop, listenAddr, "EchoClient"), time_(0) {
    client_.setConnectionCallback(
        [this](const TcpConnectionPtr& conn) { onConnection(conn); });
    client_.setMessageCallback(
        [this](const TcpConnectionPtr& conn, util::Timestamp time) {
          onMessage(conn, time);
        });
    client_.setWriteCompleteCallback(
        [](const TcpConnectionPtr&) { LOG_INFO << "WriteComplete"; });
    client_.enableRetry();
  }
  void waituntilconnect() {
    client_.connect();
    client_.already();
  }
  std::string get() { return get_; }
  void disconnect() { client_.disconnect(); }
  int recvTime() { return time_; }
  void write(const std::string message) {
    std::unique_lock lock(mutex_);
    if (conn_) {
      conn_->send(message);
    }
  }

 private:
  void onConnection(const TcpConnectionPtr& conn) {
    LOG_INFO << conn->localAddress().toIpPort() << " -> "
             << conn->peerAddress().toIpPort() << " is "
             << (conn->connected() ? "UP" : "DOWN");

    std::unique_lock lock(mutex_);
    if (conn->connected()) {
      conn_ = conn;
    } else {
      conn_.reset();
    }
  }

  void onMessage(const TcpConnectionPtr& conn, util::Timestamp time) {
    std::string msg;
    conn->readAll(msg);
    LOG_INFO << conn->name() << " recv " << msg.size() << " bytes at "
             << time.get_time();
    std::cout << msg << '\n';
    get_.append(msg);
    if (get_.size() == all.size()) cond.notify_one();
  }
  std::mutex mutex_;
  EventLoop* loop_;
  TcpClient client_;
  TcpConnectionPtr conn_;
  int time_;
  std::string get_;
};

int main(int argc, char* argv[]) {
  g_asyncLog = std::make_unique<AsyncLogging>(argv[0], kRollSize);
  gdrpc::gdlog::Logger::setOutput(
      [&](const char* msg, int len) { g_asyncLog->append(msg, len); });
  gdrpc::gdlog::Logger::setFlush([&]() { g_asyncLog->flush(); });

  LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();
  LOG_INFO << "sizeof TcpConnection = " << sizeof(TcpConnection);

  EventLoopThread loopThread;
  InetAddress addr(8080);
  EchoClient cli(loopThread.startLoop(), addr);
  cli.waituntilconnect();

  // 定义生成随机字符串的函数
  auto generateRandomString = [](size_t length) {
    static const char alphanum[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::string randomString(length, ' ');
    for (size_t i = 0; i < length; ++i) {
      randomString[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    return randomString;
  };

  int i = TIME;
  while (i--) {
    std::string s = generateRandomString(LENGTH);
    all.append(s);
    cli.write(s);
  }
  std::unique_lock lock(mutex);
  cond.wait(lock);
  std::cout << "发送接收数据" << ((cli.get() == all) ? "相同" : "不同")
            << std::endl;
  cli.disconnect();
}
