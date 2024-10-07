#ifndef _RPC_SERVER_
#define _RPC_SERVER_
#include <functional>
#include <string>
#include <unordered_map>

#include <google/protobuf/descriptor.h>

#include "../net/eventLoop.hpp"
#include "../net/inet_address.hpp"
#include "../net/tcpConnection.hpp"
#include "../net/tcpServer.hpp"
#include "../protocol/protocol.hpp"
#include "google/protobuf/service.h"
namespace gdrpc::rpc {
// 框架提供的专门发布rpc服务的网络对象类
class RpcServer {
 public:
  RpcServer(const int port, int weight = 1, const std::string ip = "localhost");
  ~RpcServer();
  // 这里是框架提供给外部使用的，可以发布rpc方法的函数接口
  void RegisterService(google::protobuf::Service* service);

  // 启动rpc服务节点，开始提供rpc远程网络调用服务
  void Run();

 private:
  void onMessage(const net::TcpConnectionPtr& conn,
                 util::Timestamp receiveTime);
  void onRpcHeader(const net::TcpConnectionPtr& conn,
                   const RpcHeaderPtr& HeaderPtr, util::Timestamp receiveTime,
                   const std::string& payload);

  // void setdoneCallback(::google::protobuf::Message* response, int64_t id);
  void SendRpcResponse(std::pair<google::protobuf::Message*, int64_t>,
                       const net::TcpConnectionPtr& conn);

  ProtocolCodec codec_;
  long id_{0};

  // std::mutex mutex_;
  // struct OutstandingCall {
  //   ::google::protobuf::RpcController* controller;
  //   ::google::protobuf::Closure* done;
  // };

  // std::map<int64_t, OutstandingCall> outstandings_;

  std::string ip_;
  int port_;
  int weight_;
  net::EventLoop eventLoop_;

  // 注册的service
  std::unordered_map<std::string, ::google::protobuf::Service*> services_;

  // 新的socket连接回调
  void onConnection(const net::TcpConnectionPtr&);
};
}  // namespace gdrpc::rpc
#endif