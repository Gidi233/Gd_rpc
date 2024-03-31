#ifndef _SOCKET_
#define _SOCKET_
#include <functional>

#include "../util/noncopyable.hpp"
namespace gdrpc {
namespace net {
class InetAddress;

// 封装socket fd
class Socket : noncopyable {
 public:
  using SockOptFunc = std::function<void(int)>;
  explicit Socket(int sockfd) : sockfd_(sockfd) {}
  ~Socket();

  int fd() const { return sockfd_; }
  void bindAddress(const InetAddress& localaddr);
  void listen();
  int accept(InetAddress* peeraddr);

  void shutdownWrite();

  void setSockOpt(SockOptFunc func) { func(sockfd_); };

 private:
  const int sockfd_;
};
}  // namespace net
}  // namespace gdrpc
#endif