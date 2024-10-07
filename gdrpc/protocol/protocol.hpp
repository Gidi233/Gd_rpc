#ifndef _PROTOCOL_
#define _PROTOCOL_
#include <cstdint>
#include <string>
#include <type_traits>

#include <google/protobuf/message.h>

#include "../net/tcpConnection.hpp"
#include "rpcHeader.pb.h"
// #include "codec.hpp"
// #include "jsonCodec.hpp"
namespace gdrpc {
namespace rpc {
// enum class protocolKind : unsigned char { Json, Protobuffer };
// enum class messageType : unsigned char {
//   Request,
//   Response,
//   Cancel,
//   Heartbeat,
//   // RequestStream,ResponseStream
// };

// enum errorCode : unsigned char {
//   NO_ERROR,
//   WRONG_PROTO,
//   NO_SERVICE,
//   NO_METHOD,
//   INVALID_REQUEST,
//   INVALID_RESPONSE,
//   TIMEOUT,
// };
// 本来打算继承 PB 的message 用于json序列化的 发现不好实现
// class MessageInfo {
//  public:
//   MessageInfo() = default;
//   std::uint32_t msg_id_;
//   std::string version_;
//   std::string service_name_;
//   std::string method_name_;
//   messageType message_Kind_;
//   errorCode error_code_;
//   std::uint32_t extendInfo_len_;
//   std::uint32_t payload_len_;
// };

// DEFINE_STRUCT_SCHEMA(MessageInfo, DEFINE_SAME_STRUCT_FIELD(msg_id_),
//                      DEFINE_SAME_STRUCT_FIELD(service_name_),
//                      DEFINE_SAME_STRUCT_FIELD(method_name_),
//                      DEFINE_SAME_STRUCT_FIELD(message_Kind_),
//                      DEFINE_SAME_STRUCT_FIELD(error_code_),
//                      DEFINE_SAME_STRUCT_FIELD(extendInfo_len_),
//                      DEFINE_SAME_STRUCT_FIELD(payload_len_));
using RpcHeaderPtr = std::shared_ptr<RpcHeader>;

class ProtocolCodec {
 public:
  enum ErrorCode {
    kNoError = 0,
    kInvalidLength,
    kCheckSumError,
    kInvalidNameLen,
    kUnknownMessageType,
    kParseError,
  };

  constexpr static uint64_t kHeaderLen = sizeof(uint32_t);
  constexpr static uint64_t kChecksumLen = sizeof(uint32_t);

  using RawMessageCallback = std::function<bool(const net::TcpConnectionPtr&,
                                                std::string, util::Timestamp)>;

  using ProtobufMessageCallback =
      std::function<void(const net::TcpConnectionPtr&, const RpcHeaderPtr&,
                         util::Timestamp, const std::string&)>;

  using ErrorCallback = std::function<void(const net::TcpConnectionPtr&,
                                           util::Timestamp, ErrorCode)>;

  ProtocolCodec(const ProtobufMessageCallback& messageCb,
                std::string tagArg = "v1.0",
                const RawMessageCallback& rawCb = RawMessageCallback())
                // const ErrorCallback& errorCb = defaultErrorCallback)
      : tag_(tagArg),
        messageCallback_(messageCb),
        rawCb_(rawCb),
        // errorCallback_(errorCb),
        kMinMessageLen(tagArg.size() + kChecksumLen) {}

  // ~ProtobufCodecLite() = default;

  const std::string& tag() const { return tag_; }

  void send(const net::TcpConnectionPtr& conn,
            const ::google::protobuf::Message& header,
            const ::google::protobuf::Message* message);

  void onMessage(const net::TcpConnectionPtr& conn,
                 util::Timestamp receiveTime);

  // virtual bool parseFromBuffer(std::string buf,
  //                              google::protobuf::Message* message);
  // virtual int serializeToBuffer(const google::protobuf::Message& message,
  //                               Buffer* buf);

  // static const string& errorCodeToString(ErrorCode errorCode);

  // public for unit tests
  // ErrorCode parse(const char* buf, int len,
  // ::google::protobuf::Message* message);
  // void fillEmptyBuffer(std::string& buf,
                      //  const google::protobuf::Message& message);

  // static int32_t checksum(const void* buf, int len);
  // static bool validateChecksum(const char* buf, int len);
  // static int32_t asInt32(const char* buf);
  // static void defaultErrorCallback(const net::TcpConnectionPtr&,
                                  //  util::Timestamp, ErrorCode);
  // const std::string errorCodeToString(ErrorCode errorCode);

 private:
  const ::google::protobuf::Message* prototype_;
  const std::string tag_;
  ProtobufMessageCallback messageCallback_;
  RawMessageCallback rawCb_;
  // ErrorCallback errorCallback_;
  const int kMinMessageLen;

 private:
  // encode();
  // decode();
  // Codec codec;

  std::uint32_t pk_len_{0};
  // protocolKind protocol_kind_;
  RpcHeader msg_info_;
  std::string extend_data_;
  std::string pb_data_;
  // std::uint32_t check_sum_{0};

  bool parse_success{false};
};

}  // namespace rpc
}  // namespace gdrpc
#endif