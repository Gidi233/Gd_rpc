#include <string>

// #include "zookeeperutil.h"
#include "../protocol/rpcHeader.pb.h"
#include "../util/log.hpp"
#include "rpcChannelCli.hpp"
namespace gdrpc::rpc {
// 所有通过stub代理对象调用的rpc方法，都走到这里了，统一做rpc方法调用的数据数据序列化和网络发送
void rpcChannelCli::CallMethod(const google::protobuf::MethodDescriptor* method,
                               google::protobuf::RpcController* controller,
                               const google::protobuf::Message* request,
                               google::protobuf::Message* response,
                               google::protobuf::Closure* done) {
  RpcHeader message;
  message.set_message_type_(Request);
  // int64_t id = id_.incrementAndGet();
  message.set_msg_id_(id_);
  message.set_service_name_(method->service()->full_name());
  message.set_method_name_(method->name());
  // message.set_request();  // FIXME: error check

  OutstandingCall out = {response, done};
  {
    std::unique_lock lock(mutex_);
    outstandings_[id_] = out;
  }
  codec_.send(conn_, message, request);
}
rpcChannelCli::rpcChannelCli(net::TcpClientPtr client,
                             const net::InetAddress& serverAddr)
    : client_(client),
      serverAddr_(serverAddr),
      // local_addr_(localAddr),
      codec_([this](const net::TcpConnectionPtr& conn,
                    const RpcHeaderPtr& headerPtr, util::Timestamp receiveTime,
                    std::string payload) {
        onRpcHeader(conn, headerPtr, receiveTime, payload);
      }) {
  client_->setConnectionCallback(
      [this](const net::TcpConnectionPtr& conn) { setConnection(conn); });
  LOG_INFO << "rpcChannelCli::ctor - " << this;
}

// rpcChannelCli::rpcChannelCli(const net::TcpConnectionPtr& conn)
//     : codec_([this](const net::TcpConnectionPtr& conn,
//                     const RpcHeaderPtr& headerPtr, util::Timestamp
//                     receiveTime, std::string payload) {
//         onRpcHeader(conn, headerPtr, receiveTime, payload);
//       }),
//       conn_(conn),
//       services_(nullptr) {
//   LOG_INFO << "rpcChannelCli::ctor - " << this;
// }

rpcChannelCli::~rpcChannelCli() {
  LOG_INFO << "rpcChannelCli::dtor - " << this;
  for (const auto& outstanding : outstandings_) {
    OutstandingCall out = outstanding.second;
    delete out.response;
    delete out.done;
  }
}

void rpcChannelCli::onMessage(const net::TcpConnectionPtr& conn,
                              util::Timestamp receiveTime) {
  codec_.onMessage(conn, receiveTime);
}
void rpcChannelCli::onRpcHeader(const net::TcpConnectionPtr& conn,
                                const RpcHeaderPtr& headerPtr,
                                util::Timestamp receiveTime,
                                std::string payload) {
  RpcHeader& message = *headerPtr;

  switch (message.message_type_()) {
    case Response: {
      int64_t id = message.msg_id_();

      OutstandingCall out = {nullptr, nullptr};

      {
        std::unique_lock lock(mutex_);
        std::map<int64_t, OutstandingCall>::iterator it =
            outstandings_.find(id);
        if (it != outstandings_.end()) {
          out = it->second;
          outstandings_.erase(it);
        }
      }

      if (out.response) {
        std::unique_ptr<google::protobuf::Message> d(out.response);
        out.response->ParseFromString(payload);
        if (out.done) {
          out.done->Run();
        }
      }
    } break;
    default:
      break;
  }
}
// std::shared_ptr<hsrpc::TinyPBProtocol> req_protocol =
//     std::make_shared<hsrpc::TinyPBProtocol>();

// RpcController* my_controller =
// dynamic_cast<RpcController*>(controller); if (my_controller == nullptr)
// {
//   LOG_ERROR << "failed callmethod, RpcController convert error";
//   return;
// }

// if (my_controller->GetMsgId().empty()) {
//   req_protocol->msg_id_ = MsgIDUtil::GenMsgID();
//   my_controller->SetMsgId(req_protocol->msg_id_);
// } else {
//   req_protocol->msg_id_ = my_controller->GetMsgId();
// }

// req_protocol->method_name_ = method->full_name();
// INFOLOG("%s | call method name [%s]", req_protocol->msg_id_.c_str(),
//         req_protocol->method_name_.c_str());

// if (!is_init_) {
//   std::string err_info = "RpcChannel not init";
//   my_controller->SetError(ERROR_RPC_CHANNEL_INIT, err_info);
//   ERRORLOG("%s | %s, RpcChannel not init ",
//   req_protocol->msg_id_.c_str(),
//            err_info.c_str());
//   return;
// }

// // requeset 的序列化
// if (!request->SerializeToString(&(req_protocol->pb_data_))) {
//   std::string err_info = "failde to serialize";
//   my_controller->SetError(ERROR_FAILED_SERIALIZE, err_info);
//   ERRORLOG("%s | %s, origin requeset [%s] ",
//   req_protocol->msg_id_.c_str(),
//            err_info.c_str(), request->ShortDebugString().c_str());
//   return;
// }

// s_ptr channel = shared_frothis_();

// timer_event_ = std::make_shared<TimerEvent>(
//     my_controller->GetTimeout(), false, [my_controller, channel]()
//     mutable
//     {
//       my_controller->StartCancel();
//       my_controller->SetError(
//           ERROR_RPC_CALL_TIMEOUT,
//           "rpc call timeout " +
//           std::to_string(my_controller->GetTimeout()));

//       if (channel->getClosure()) {
//         channel->getClosure()->Run();
//       }
//       channel.reset();
//     });

// client_->addTimerEvent(timer_event_);

// client_->connect([req_protocol, channel]() mutable {
//   RpcController* my_controller =
//       dynamic_cast<RpcController*>(channel->getController());

//   if (channel->getTcpClient()->getConnectErrorCode() != 0) {
//     my_controller->SetError(channel->getTcpClient()->getConnectErrorCode(),
//                             channel->getTcpClient()->getConnectErrorInfo());
//     ERRORLOG(
//         "%s | connect error, error coode[%d], error info[%s], peer
//         addr[%s]", req_protocol->msg_id_.c_str(),
//         my_controller->GetErrorCode(),
//         my_controller->GetErrorInfo().c_str(),
//         channel->getTcpClient()->getPeerAddr()->toString().c_str());
//     return;
//   }

//   INFOLOG("%s | connect success, peer addr[%s], local addr[%s]",
//           req_protocol->msg_id_.c_str(),
//           channel->getTcpClient()->getPeerAddr()->toString().c_str(),
//           channel->getTcpClient()->getLocalAddr()->toString().c_str());

//   channel->getTcpClient()->writeMessage(
//       req_protocol, [req_protocol, channel,
//                      my_controller](AbstractProtocol::s_ptr) mutable {
//         INFOLOG(
//             "%s | send rpc requl> req_protocol =
//     std::make_shared<hsrpc::TinyPBProtocol>();

// RpcController* my_controller =
// dynamic_cast<RpcController*>(controller); if (my_controller == nullptr)
// {
//   LOG_ERROR << "failed callmethod, RpcController convert error";
//   return;
// }

// if (my_controller->GetMsgId().empty()) {
//   req_protocol->msg_id_ = MsgIDUtil::GenMsgID();
//   my_controller->SetMsgId(req_protocol->msg_id_);
// } else {
//   req_protocol->msg_id_ = my_controller->GetMsgId();
// }

// req_protocol->method_name_ = method->full_name();
// INFOLOG("%s | call method name [%s]", req_protocol->msg_id_.c_str(),
//         req_protocol->method_name_.c_str());

// if (!is_init_) {
//   std::string err_info = "RpcChannel not init";
//   my_controller->SetError(ERROR_RPC_CHANNEL_INIT, err_info);
//   ERRORLOG("%s | %s, RpcChannel not init ",
//   req_protocol->msg_id_.c_str(),
//            err_info.c_str());
//   return;
// }

// // requeset 的序列化
// if (!request->SerializeToString(&(req_protocol->pb_data_))) {
//   std::string err_info = "failde to serialize";
//   my_controller->SetError(ERROR_FAILED_SERIALIZE, err_info);
//   ERRORLOG("%s | %s, origin requeset [%s] ",
//   req_protocol->msg_id_.c_str(),
//            err_info.c_str(), request->ShortDebugString().c_str());
//   return;
// }

// s_ptr channel = shared_frothis_();

// timer_event_ = std::make_shared<TimerEvent>(
//     my_controller->GetTimeout(), false, [my_controller, channel]()
//     mutable
//     {
//       my_controller->StartCancel();
//       my_controller->SetError(
//           ERROR_RPC_CALL_TIMEOUT,
//           "rpc call timeout " +
//           std::to_string(my_controller->GetTimeout()));

//       if (channel->getClosure()) {
//         channel->getClosure()->Run();
//       }
//       channel.reset();
//     });

// client_->addTimerEvent(timer_event_);

// client_->connect([req_protocol, channel]() mutable {
//   RpcController* my_controller =
//       dynamic_cast<RpcController*>(channel->getController());

//   if (channel->getTcpClient()->getConnectErrorCode() != 0) {
//     my_controller->SetError(channel->getTcpClient()->getConnectErrorCode(),
//                             channel->getTcpClient()->getConnectErrorInfo());
//     ERRORLOG(
//         "%s | connect error, error coode[%d], error info[%s], peer
//         addr[%s]", req_protocol->msg_id_.c_str(),
//         my_controller->GetErrorCode(),
//         my_controller->GetErrorInfo().c_str(),
//         channel->getTcpClient()->getPeerAddr()->toString().c_str());
//     return;
//   }

//   INFOLOG("%s | connect success, peer addr[%s], local addr[%s]",
//           req_protocol->msg_id_.c_str(),
//           channel->getTcpClient()->getPeerAddr()->toString().c_str(),
//           channel->getTcpClient()->getLocalAddr()->toString().c_str());

//   channel->getTcpClient()->writeMessage(
//       req_protocol, [req_protocol, channel,
//                      my_controller](AbstractProtocol::s_ptr) mutable {
//         INFOLOG(
//             "%s | send rpc request success. call method name[%s], peer
//             " "addr[%s], local addr[%s]",
//             req_protocol->msg_id_.c_str(),
//             req_protocol->method_name_.c_str(),
//             channel->getTcpClient()->getPeerAddr()->toString().c_str(),
//             channel->getTcpClient()->getLocalAddr()->toString().c_str());

//         channel->getTcpClient()->readMessage(
//             req_protocol->msg_id_,
//             [channel, my_controller](AbstractProtocol::s_ptr msg)
//             mutable {
//               std::shared_ptr<hsrpc::TinyPBProtocol> rsp_protocol =
//                   std::dynamic_pointer_cast<hsrpc::TinyPBProtocol>(msg);
//               INFOLOG(
//                   "%s | success get rpc response, call method name[%s],
//                   peer " "addr[%s], local addr[%s]",
//                   rsp_protocol->msg_id_.c_str(),
//                   rsp_protocol->method_name_.c_str(),
//                   channel->getTcpClient()->getPeerAddr()->toString().c_str(),
//                   channel->getTcpClient()
//                       ->getLocalAddr()
//                       ->toString()
//                       .c_str());

//               // 当成功读取到回包后， 取消定时任务
//               channel->getTimerEvent()->setCancled(true);

//               if (!(channel->getResponse()->ParseFromString(
//                       rsp_protocol->pb_data_))) {
//                 ERRORLOG("%s | serialize error",
//                          rsp_protocol->msg_id_.c_str());
//                 my_controller->SetError(ERROR_FAILED_SERIALIZE,
//                                         "serialize error");
//                 return;
//               }

//               if (rsp_protocol->err_code_ != 0) {
//                 ERRORLOG(
//                     "%s | call rpc methood[%s] failed, error code[%d],
//                     error " "info[%s]", rsp_protocol->msg_id_.c_str(),
//                     rsp_protocol->method_name_.c_str(),
//                     rsp_protocol->err_code_,
//                     rsp_protocol->err_info_.c_str());

//                 my_controller->SetError(rsp_protocol->err_code_,
//                                         rsp_protocol->err_info_);
//                 return;
//               }

//               INFOLOG(
//                   "%s | call rpc success, call method name[%s], peer "
//                   "addr[%s], local addr[%s]",
//                   rsp_protocol->msg_id_.c_str(),
//                   rsp_protocol->method_name_.c_str(),
//                   channel->getTcpClient()->getPeerAddr()->toString().c_str(),
//                   channel->getTcpClient()->getLocalAddr()->toString().c_str())

//               if (!my_controller->IsCanceled() &&
//               channel->getClosure()) {
//                 channel->getClosure()->Run();
//               }

//               // channel.reset();
//             });
//       });
// });d_.c_str(), req_protocol->method_name_.c_str(),
//             channel->getTcpClient()->getPeerAddr()->toString().c_str(),
//             channel->getTcpClient()->getLocalAddr()->toString().c_str());

//         channel->getTcpClient()->readMessage(
//             req_protocol->msg_id_,
//             [channel, my_controller](AbstractProtocol::s_ptr msg)
//             mutable {
//               std::shared_ptr<hsrpc::TinyPBProtocol> rsp_protocol =
//                   std::dynamic_pointer_cast<hsrpc::TinyPBProtocol>(msg);
//               INFOLOG(
//                   "%s | success get rpc response, call method name[%s],
//                   peer " "addr[%s], local addr[%s]",
//                   rsp_protocol->msg_id_.c_str(),
//                   rsp_protocol->method_name_.c_str(),
//                   channel->getTcpClient()->getPeerAddr()->toString().c_str(),
//                   channel->getTcpClient()
//                       ->getLocalAddr()
//                       ->toString()
//                       .c_str());

//               // 当成功读取到回包后， 取消定时任务
//               channel->getTimerEvent()->setCancled(true);

//               if (!(channel->getResponse()->ParseFromString(
//                       rsp_protocol->pb_data_))) {
//                 ERRORLOG("%s | serialize error",
//                          rsp_protocol->msg_id_.c_str());
//                 my_controller->SetError(ERROR_FAILED_SERIALIZE,
//                                         "serialize error");
//                 return;
//               }

//               if (rsp_protocol->err_code_ != 0) {
//                 ERRORLOG(
//                     "%s | call rpc methood[%s] failed, error code[%d],
//                     error " "info[%s]", rsp_protocol->msg_id_.c_str(),
//                     rsp_protocol->method_name_.c_str(),
//                     rsp_protocol->err_code_,
//                     rsp_protocol->err_info_.c_str());

//                 my_controller->SetError(rsp_protocol->err_code_,
//                                         rsp_protocol->err_info_);
//                 return;
//               }

//               INFOLOG(
//                   "%s | call rpc success, call method name[%s], peer "
//                   "addr[%s], local addr[%s]",
//                   rsp_protocol->msg_id_.c_str(),
//                   rsp_protocol->method_name_.c_str(),
//                   channel->getTcpClient()->getPeerAddr()->toString().c_str(),
//                   channel->getTcpClient()->getLocalAddr()->toString().c_str())

//               if (!my_controller->IsCanceled() &&
//               channel->getClosure()) {
//                 channel->getClosure()->Run();
//               }

//               // channel.reset();
//             });
//       });
// });
// }
}  // namespace gdrpc::rpc