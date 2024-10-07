#ifndef _RPC_CONTROLLER_
#define _RPC_CONTROLLER_

#include <string>

#include <google/protobuf/service.h>
#include <google/protobuf/stubs/callback.h>

#include "../net/inet_address.hpp"

namespace gdrpc::rpc {

class RpcController : public google::protobuf::RpcController {
 public:
  RpcController() {}
  ~RpcController() {}

  void Reset();

  bool Failed() const;

  std::string ErrorText() const;

  void StartCancel();

  void SetFailed(const std::string& reason);

  bool IsCanceled() const;

  void NotifyOnCancel(google::protobuf::Closure* callback);

  void SetError(int32_t error_code, const std::string error_info);

  int32_t GetErrorCode();

  std::string GetErrorInfo();

  void SetMsgId(const std::string& msg_id);

  std::string GetMsgId();

  void SetLocalAddr(net::InetAddress addr);

  void SetPeerAddr(net::InetAddress addr);

  net::InetAddress GetLocalAddr();

  net::InetAddress GetPeerAddr();

  void SetTimeout(int timeout);

  int GetTimeout();

 private:
  int32_t error_code_{0};
  std::string error_info_;
  std::string msg_id_;

  bool is_failed_{false};
  bool is_cancled_{false};

  net::InetAddress local_addr_;
  net::InetAddress peer_addr_;

  int timeout_{1000};  // ms
};

}  // namespace gdrpc::rpc

#endif