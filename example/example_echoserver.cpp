#include <unistd.h>

#include <utility>

#include "../gdrpc/net/callback.hpp"
#include "../gdrpc/net/eventLoop.hpp"
#include "../gdrpc/net/tcpConnection.hpp"
#include "../gdrpc/net/tcpServer.hpp"
#include "../gdrpc/util/async_log.hpp"
#include "../gdrpc/util/log.hpp"
#include "../gdrpc/util/timestamp.hpp"
using namespace gdrpc;
using namespace gdrpc::net;
using namespace gdrpc::gdlog;

#define kRollSize (20 * 1024 * 1024)

int numThreads = 0;
std::unique_ptr<gdrpc::gdlog::AsyncLogging> g_asyncLog;

class EchoServer {
 public:
  EchoServer(EventLoop* loop, const InetAddress& listenAddr)
      : loop_(loop), server_(loop, listenAddr, "EchoServer") {
    server_.setConnectionCallback(
        [this](const TcpConnectionPtr& conn) { onConnection(conn); });
    server_.setMessageCallback(
        [this](const TcpConnectionPtr& conn, util::Timestamp time) {
          onMessage(conn, time);
        });
    server_.setThreadNum(numThreads);
  }

  void start() { server_.start(); }
  // void stop();

 private:
  void onConnection(const TcpConnectionPtr& conn) {
    LOG_DEBUG << conn->peerAddress().toIpPort() << " -> "
              << conn->localAddress().toIpPort() << " is "
              << (conn->connected() ? "UP" : "DOWN");
    LOG_INFO << conn->name();
  }

  void onMessage(const TcpConnectionPtr& conn, util::Timestamp time) {
    std::string msg;
    conn->readAll(msg);
    LOG_INFO << conn->name() << " recv " << msg.size() << " bytes at "
             << time.get_time();
    if (msg == "exit\n") {
      conn->send("bye\n");
      conn->shutdown();
    }
    if (msg == "quit\n") {
      loop_->quit();
    }
    conn->send(msg);
  }

  EventLoop* loop_;
  TcpServer server_;
};

int main(int argc, char* argv[]) {
  g_asyncLog = std::make_unique<AsyncLogging>(argv[0], kRollSize);
  gdrpc::gdlog::Logger::setOutput(
      [&](const char* msg, int len) { g_asyncLog->append(msg, len); });
  gdrpc::gdlog::Logger::setFlush([&]() { g_asyncLog->flush(); });

  LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();
  LOG_INFO << "sizeof TcpConnection = " << sizeof(TcpConnection);
  if (argc > 1) {
    numThreads = atoi(argv[1]);
  }

  EventLoop loop;
  InetAddress listenAddr(8080);
  EchoServer server(&loop, listenAddr);

  server.start();

  loop.loop();
}
