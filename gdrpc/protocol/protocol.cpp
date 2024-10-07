#include "protocol.hpp"

#include <sstream>

#include "../util/log.hpp"
namespace gdrpc::rpc {

void ProtocolCodec::send(const net::TcpConnectionPtr& conn,
                         const ::google::protobuf::Message& header,
                         const ::google::protobuf::Message* message) {
  // FIXME: serialize to TcpConnection::outputBuffer()
  std::string head = header.SerializeAsString();
  std::string payload = message->SerializeAsString();
  int len = head.size() + payload.size();
  std::stringstream stream;
  stream.write(reinterpret_cast<const char*>(&len), sizeof len);
  stream << head << payload;
  conn->send(stream.str());
}
// 真的需要checksum吗

// void ProtocolCodec::fillEmptyBuffer(std::string& buf,
//                                     const google::protobuf::Message& message)
//                                     {
//   // FIXME: can we move serialization & checksum to other thread?
//   //   buf->append(tag_);

//   // int byte_size = serializeToBuffer(message, buf);

//   buf->ensureWritableBytes(byte_size + kChecksumLen);

//   // uint8_t* start = reinterpret_cast<uint8_t*>(buf->beginWrite());
//   // uint8_t* end = message.SerializeWithCachedSizesToArray(start);

//   // int32_t checkSum = checksum(buf->peek(),
//   // static_cast<int>(buf->readableBytes())); buf->appendInt32(checkSum);
//   // assert(buf->readableBytes() == tag_.size() + byte_size + kChecksumLen);
//   // (void)byte_size;
//   // int32_t len =
//   // sockets::hostToNetwork32(static_cast<int32_t>(buf->readableBytes()));
//   buf->prepend(&len, sizeof len);
// }

void ProtocolCodec::onMessage(const net::TcpConnectionPtr& conn,
                              util::Timestamp receiveTime) {
  long len;
  std::string buf;
  while ((len = conn->tryRead(buf)) >=
         static_cast<size_t>(kMinMessageLen + kHeaderLen)) {
    int32_t headerLen;
    memcpy(&headerLen, buf.data(), sizeof(headerLen));
    if (buf.size() >= static_cast<size_t>(kHeaderLen + headerLen)) {
      // if (rawCb_ && !rawCb_(conn, std::string(buf->peek(), kHeaderLen + len),
      //                       receiveTime)) {
      //   buf->retrieve(kHeaderLen + len);
      //   continue;
      // }
      RpcHeaderPtr header;
      header->ParseFromArray(buf.data() + sizeof headerLen, headerLen);
      auto payloadLen = header->payload_len_();
      // FIXME: can we move deserialization & callback to other thread?
      // ErrorCode errorCode = parse(buf->peek() + kHeaderLen, len,
      // header.get()); if (errorCode == kNoError) {
      // FIXME: try { } catch (...) { }
      std::string payload(buf.data() + sizeof headerLen + kHeaderLen,
                          payloadLen);
      if (buf.size() >=
          static_cast<size_t>(kHeaderLen + headerLen + payloadLen)) {
        messageCallback_(conn, header, receiveTime, payload);
        conn->retrieve(kHeaderLen + len + payloadLen);
      }
      // } else {
      // errorCallback_(conn, buf, receiveTime, errorCode);
      // break;
      // }
    } else {
      break;
    }
  }
}

// bool ProtocolCodec::parseFromBuffer(std::string buf,
//                                     google::protobuf::Message* message) {
//   return message->ParseFromArray(buf.data(), buf.size());
// }

// int ProtocolCodec::serializeToBuffer(const google::protobuf::Message&
// message,
//                                      Buffer* buf) {
//   // TODO: use BufferOutputStream
//   // BufferOutputStream os(buf);
//   // message.SerializeToZeroCopyStream(&os);
//   // return static_cast<int>(os.ByteCount());

//   // code copied from MessageLite::SerializeToArray() and
//   // MessageLite::SerializePartialToArray().
//   GOOGLE_DCHECK(message.IsInitialized())
//       << InitializationErrorMessage("serialize", message);

//   /**
//    * 'ByteSize()' of message is deprecated in Protocol Buffers v3.4.0
//    firstly.
//    * But, till to v3.11.0, it just getting start to be marked by
//    * '__attribute__((deprecated()))'. So, here, v3.9.2 is selected as maximum
//    * version using 'ByteSize()' to avoid potential effect for previous muduo
//    * code/projects as far as possible. Note: All information above just INFER
//    * from 1) https://github.com/protocolbuffers/protobuf/releases/tag/v3.4.0
//    2)
//    * MACRO in file 'include/google/protobuf/port_def.inc'. eg. '#define
//    * PROTOBUF_DEPRECATED_MSG(msg) __attribute__((deprecated(msg)))'. In
//    * addition, usage of 'ToIntSize()' comes from Impl of ByteSize() in new
//    * version's Protocol Buffers.
//    */

// #if GOOGLE_PROTOBUF_VERSION > 3009002
//   int byte_size =
//   google::protobuf::internal::ToIntSize(message.ByteSizeLong());
// #else
//   int byte_size = message.ByteSize();
// #endif
//   buf->ensureWritableBytes(byte_size + kChecksumLen);

//   uint8_t* start = reinterpret_cast<uint8_t*>(buf->beginWrite());
//   uint8_t* end = message.SerializeWithCachedSizesToArray(start);
//   if (end - start != byte_size) {
// #if GOOGLE_PROTOBUF_VERSION > 3009002
//     ByteSizeConsistencyError(
//         byte_size,
//         google::protobuf::internal::ToIntSize(message.ByteSizeLong()),
//         static_cast<int>(end - start));
// #else
//     ByteSizeConsistencyError(byte_size, message.ByteSize(),
//                              static_cast<int>(end - start));
// #endif
//   }
//   buf->hasWritten(byte_size);
//   return byte_size;
// }
// constexpr static const std::array<std::string_view, 7> errorMessages = {
//     {"NoError", "InvalidLength", "CheckSumError", "InvalidNameLen",
//      "UnknownMessageType", "ParseError", "UnknownError"}};
// constexpr static std::string_view errorCodeToString(ErrorCode errorCode) {
//   return errorMessages[errorCode];
// }

// void ProtocolCodec::defaultErrorCallback(const net::TcpConnectionPtr& conn,
//                                          util::Timestamp, ErrorCode
//                                          errorCode) {
//   auto e = errorCodeToString(errorCode);
//   LOG_FATAL << "ProtocolCodec::defaultErrorCallback - "
//             << errorCodeToString(errorCode);
//   if (conn && conn->connected()) {
//     conn->shutdown();
//   }
// }

// int32_t ProtocolCodec::asInt32(const char* buf) {
//   int32_t be32 = 0;
//   ::memcpy(&be32, buf, sizeof(be32));
//   return sockets::networkToHost32(be32);
// }

// int32_t ProtocolCodec::checksum(const void* buf, int len) {
//   return static_cast<int32_t>(
//       ::adler32(1, static_cast<const Bytef*>(buf), len));
// }

// bool ProtocolCodec::validateChecksum(const char* buf, int len) {
//   // check sum
//   int32_t expectedCheckSum = asInt32(buf + len - kChecksumLen);
//   int32_t checkSum = checksum(buf, len - kChecksumLen);
//   return checkSum == expectedCheckSum;
// }

// ProtocolCodec::ErrorCode ProtocolCodec::parse(
//     const char* buf, int len, ::google::protobuf::Message* message) {
//   ErrorCode error = kNoError;

//   if (validateChecksum(buf, len)) {
//     if (memcmp(buf, tag_.data(), tag_.size()) == 0) {
//       // parse from buffer
//       const char* data = buf + tag_.size();
//       int32_t dataLen = len - kChecksumLen - static_cast<int>(tag_.size());
//       if (parseFromBuffer(std::string(data, dataLen), message)) {
//         error = kNoError;
//       } else {
//         error = kParseError;
//       }
//     } else {
//       error = kUnknownMessageType;
//     }
//   } else {
//     error = kCheckSumError;
//   }

//   return error;
// }

}  // namespace gdrpc::rpc