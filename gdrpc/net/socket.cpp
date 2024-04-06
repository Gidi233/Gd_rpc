#include "socket.hpp"

#include <netinet/tcp.h>
#include <string.h>
#include <unistd.h>

#include "../util/log.hpp"
#include "inet_address.hpp"

namespace gdrpc {
namespace net {
Socket::~Socket() { ::close(sockfd_); }

void Socket::bindAddress(const InetAddress& localaddr) {
  if (0 != ::bind(sockfd_,
                  reinterpret_cast<sockaddr*>(
                      const_cast<sockaddr_in*>(localaddr.getSockAddr())),
                  sizeof(sockaddr_in))) {
    LOG_FATAL << "bind sockfd:" << sockfd_ << " fail";
  }
}

void Socket::listen() {
  if (0 != ::listen(sockfd_, 1024)) {
    LOG_FATAL << "listen sockfd:" << sockfd_ << " fail";
  }
}

int Socket::accept(InetAddress* peeraddr) {
  sockaddr_in addr;
  socklen_t len = sizeof(addr);
  ::memset(&addr, 0, sizeof(addr));
  int connfd =
      ::accept4(sockfd_, (sockaddr*)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
  if (connfd >= 0) {
    peeraddr->setSockAddr(addr);
  }
  return connfd;
}

}  // namespace net
}  // namespace gdrpc