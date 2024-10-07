#include <iostream>
#include <string>
#include <vector>

#include "../gdrpc/protocol/jsonCodec.hpp"
using namespace gdrpc::rpc;
using nlohmann::json;

enum class messageType : unsigned char {
  Request,
  Response,
  Cancel,
  Heartbeat,
  // RequestStream,ResponseStream
};
//
enum errorCode : unsigned char {
  NO_ERROR,
  WRONG_PROTO,
  NO_SERVICE,
  NO_METHOD,
  INVALID_REQUEST,
  INVALID_RESPONSE,
  TIMEOUT,
};
// 本来打算继承 PB 的message 用于json序列化的 发现不好实现
struct MessageInfo {
  std::uint32_t msg_id_;
  std::string service_name_;
  std::string method_name_;
  messageType message_Kind_;
  errorCode error_code_;
  std::uint32_t extendInfo_len_;
};
//
DEFINE_STRUCT_SCHEMA(MessageInfo, DEFINE_SAME_STRUCT_FIELD(msg_id_),
                     DEFINE_SAME_STRUCT_FIELD(service_name_),
                     DEFINE_SAME_STRUCT_FIELD(method_name_),
                     DEFINE_SAME_STRUCT_FIELD(message_Kind_),
                     DEFINE_SAME_STRUCT_FIELD(error_code_),
                     DEFINE_SAME_STRUCT_FIELD(extendInfo_len_))
int main() {
  std::cout << json(json::parse("{"
                                "  \"msg_id_\": 1,"
                                "  \"service_name_\": \"foo\","
                                "  \"method_name_\": \"foo\","
                                "  \"message_Kind_\": 2,"
                                "  \"error_code_\": 2,"
                                "  \"extendInfo_len_\": 2"
                                "}")
                        .get<MessageInfo>())
                   .dump(2)
            << std::endl;
  return 0;
}