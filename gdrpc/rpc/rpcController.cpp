
#include "rpcController.hpp"

namespace gdrpc::rpc {

void RpcController::Reset() {
  error_code_ = 0;
  error_info_ = "";
  msg_id_ = "";
  is_failed_ = false;
  is_cancled_ = false;
  // local_addr_ = nullptr;
  // peer_addr_ = nullptr;
  timeout_ = 1000;  // ms
}

bool RpcController::Failed() const { return is_failed_; }

std::string RpcController::ErrorText() const { return error_info_; }

void RpcController::StartCancel() { is_cancled_ = true; }

void RpcController::SetFailed(const std::string& reason) {
  error_info_ = reason;
}

bool RpcController::IsCanceled() const { return is_cancled_; }

void RpcController::NotifyOnCancel(google::protobuf::Closure* callback) {}

void RpcController::SetError(int32_t error_code, const std::string error_info) {
  error_code_ = error_code;
  error_info_ = error_info;
  is_failed_ = true;
}

int32_t RpcController::GetErrorCode() { return error_code_; }

std::string RpcController::GetErrorInfo() { return error_info_; }

void RpcController::SetMsgId(const std::string& msg_id) { msg_id_ = msg_id; }

std::string RpcController::GetMsgId() { return msg_id_; }

void RpcController::SetLocalAddr(net::InetAddress addr) { local_addr_ = addr; }

void RpcController::SetPeerAddr(net::InetAddress addr) { peer_addr_ = addr; }

net::InetAddress RpcController::GetLocalAddr() { return local_addr_; }

net::InetAddress RpcController::GetPeerAddr() { return peer_addr_; }

void RpcController::SetTimeout(int timeout) { timeout_ = timeout; }

int RpcController::GetTimeout() { return timeout_; }

}  // namespace gdrpc::rpc