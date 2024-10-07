#pragma once

#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <google/protobuf/service.h>

#include "../net/inet_address.hpp"
#include "../net/tcpClient.hpp"
#include "../protocol/protocol.hpp"
namespace gdrpc::rpc {

class rpcChannelCli : public google::protobuf::RpcChannel {
  using Functor = std::function<void()>;

 public:
  //  传入超时事件，callmethod里刷新
  rpcChannelCli(net::TcpClientPtr client, const net::InetAddress& serverAddr);
  ~rpcChannelCli();
  // 所有通过stub代理对象调用的rpc方法，都走到这里了，统一做rpc方法调用的数据数据序列化和网络发送
  void CallMethod(const google::protobuf::MethodDescriptor* method,
                  google::protobuf::RpcController* controller,
                  const google::protobuf::Message* request,
                  google::protobuf::Message* response,
                  google::protobuf::Closure* done);

  // std::optional<std::weak_ptr<gdrpc::util::Task>> addTimeEvent(int64_t
  // afterMs,
  //  Functor cb) {
  // client_.getLoop()->runAfter(afterMs, cb);
  // }

 private:
  void onMessage(const net::TcpConnectionPtr& conn,
                 util::Timestamp receiveTime);
  void setConnection(const net::TcpConnectionPtr& conn) { conn_ = conn; }
  void onRpcHeader(const net::TcpConnectionPtr& conn,
                   const RpcHeaderPtr& HeaderPtr, util::Timestamp receiveTime,
                   std::string payload);

 private:
  net::InetAddress serverAddr_;  //
  // net::InetAddress local_addr_;

  ProtocolCodec codec_;
  net::TcpConnectionPtr conn_;
  long id_{0};

  std::mutex mutex_;
  struct OutstandingCall {
    ::google::protobuf::Message* response;
    ::google::protobuf::Closure* done;
  };

  std::map<int64_t, OutstandingCall> outstandings_;

  // controller_s_ptr controller_{nullptr};
  // message_s_ptr request_{nullptr};
  // message_s_ptr response_{nullptr};
  // closure_s_ptr closure_{nullptr};

  net::TcpClientPtr client_;
};
using rpcChannelCliPtr = std::shared_ptr<rpcChannelCli>;
}  // namespace gdrpc::rpc