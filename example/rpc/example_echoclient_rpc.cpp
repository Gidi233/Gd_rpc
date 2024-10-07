
#include <iostream>
#include <mutex>

#include "../gdrpc/net/callback.hpp"
#include "../gdrpc/net/eventLoop.hpp"
#include "../gdrpc/net/eventLoop_thread.hpp"
#include "../gdrpc/net/tcpClient.hpp"
#include "../gdrpc/net/tcpConnection.hpp"
#include "../gdrpc/rpc/rpcChannelCli.hpp"
#include "../gdrpc/util/async_log.hpp"
#include "../gdrpc/util/log.hpp"
#include "../gdrpc/util/timestamp.hpp"
#include "echo.pb.h"
using namespace gdrpc;
using namespace gdrpc::net;
using namespace gdrpc::rpc;
using namespace gdrpc::gdlog;
using namespace std;
#define LENGTH 10
#define TIME 10000
#define kRollSize (20 * 1024 * 1024)

std::unique_ptr<gdrpc::gdlog::AsyncLogging> g_asyncLog;
std::condition_variable cond;
std::mutex mutex;
std::string all;
class EchoRpcClient {
 public:
  EchoRpcClient(EventLoop* loop, const InetAddress& serverAddr)
      : loop_(loop),
        client_(make_shared<TcpClient>(loop, serverAddr, "EchoRpcClient")),
        time_(0),
        channel_(TcpClientPtr(client_), serverAddr),
        stub_(&channel_) {
    client_->enableRetry();
    client_->connect();
  }
  void echo(string& information) {
    Echo::HelloMsg msg;
    Echo::HelloMsg* response = new Echo::HelloMsg;
    msg.set_id(time_++);
    msg.set_information(information);
    stub_.echo(nullptr, &msg, response,
               NewCallback(this, &EchoRpcClient::cb, response));
  }
  void wait() { client_->already(); }
  void disconnect() { client_->disconnect(); }

 private:
  void cb(Echo::HelloMsg* response) {
    cout << "id = " << response->id()
         << ", information = " << response->information() << endl;
    delete response;
  }
  EventLoop* loop_;
  TcpConnectionPtr conn_;
  int time_;
  std::string get_;

  TcpClientPtr client_;
  rpcChannelCli channel_;
  Echo::EchoService::Stub stub_;
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
  EchoRpcClient cli(loopThread.startLoop(), addr);

  cli.wait();
  auto generateRandomString = [](size_t length) {
    static const char alphanum[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::string randomString(length, ' ');
    for (size_t i = 0; i < length; ++i) {
      randomString[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    return randomString;
  };
  for (int i = 0; i < 5; i++) {
    auto s = generateRandomString(10);
    cli.echo(s);
  }
  cli.disconnect();
}
