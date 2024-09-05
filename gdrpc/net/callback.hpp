#ifndef _CALLBACK_
#define _CALLBACK_
#include <functional>
#include <memory>
// 因为在不同的命名空间不能只前项声明
#include "../util/timestamp.hpp"
namespace gdrpc {
namespace net {
class TcpConnection;

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
using CloseCallback = std::function<void(const TcpConnectionPtr&)>;
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;
using HighWaterMarkCallback =
    std::function<void(const TcpConnectionPtr&, size_t)>;

using MessageCallback =
    std::function<void(const TcpConnectionPtr&, util::Timestamp)>;
}  // namespace net
}  // namespace gdrpc
#endif