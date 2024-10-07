#include "../../gdrpc/rpc/rpcServer.hpp"
#include "../gdrpc/util/async_log.hpp"
#include "../gdrpc/util/log.hpp"
#include "../gdrpc/util/timestamp.hpp"
#include "echo.pb.h"
using namespace std;
using namespace gdrpc;
using namespace gdrpc::net;
using namespace gdrpc::rpc;
using namespace gdrpc::gdlog;

#define kRollSize (20 * 1024 * 1024)

int numThreads = 0;
std::unique_ptr<gdrpc::gdlog::AsyncLogging> g_asyncLog;

class EchoService
    : public Echo::EchoService  // 使用在rpc服务发布端（rpc服务提供者）
{
 public:
  void echo(::google::protobuf::RpcController* controller,
            const Echo::HelloMsg* request, Echo::HelloMsg* response,
            ::google::protobuf::Closure* done) override {
    cout << "hello from client, id:" << request->id()
         << " msg: " << request->information() << endl;
    response->set_id(request->id());
    response->set_information(request->information() + "back from server");

    // 执行回调操作   执行响应对象数据的序列化和网络发送（都是由框架来完成的）
    done->Run();
  }
};

int main(int argc, char* argv[]) {
  g_asyncLog = std::make_unique<AsyncLogging>(argv[0], kRollSize);
  gdrpc::gdlog::Logger::setOutput(
      [&](const char* msg, int len) { g_asyncLog->append(msg, len); });
  gdrpc::gdlog::Logger::setFlush([&]() { g_asyncLog->flush(); });

  if (argc > 1) {
    numThreads = atoi(argv[1]);
  }

  RpcServer server(8080);
  server.RegisterService(new EchoService());
  server.Run();
}
