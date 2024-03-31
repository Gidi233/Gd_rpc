#include "acceptor.hpp"

#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "../util/log.hpp"
#include "inet_address.hpp"

namespace gdrpc {
namespace net {
static int createNonblocking() {
  int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
                        IPPROTO_TCP);
  if (sockfd < 0) {
    LOG_FATAL << " listen socket create err:" << ERR_MSG;
  }
  return sockfd;
}

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr,
                   bool reuseport)
    : loop_(loop),
      acceptSocket_(createNonblocking()),
      acceptChannel_(loop, acceptSocket_.fd()),
      listenning_(false) {
  acceptSocket_.setSockOpt([](int sockfd) {
    int optval = 1;
    ::setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
    ::setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
  });
  acceptSocket_.bindAddress(listenAddr);
  acceptChannel_.setReadCallback([&](util::Timestamp ts) { handleRead(); });
}

Acceptor::~Acceptor() {
  // 调用EventLoop->removeChannel =>
  // Poller->removeChannel把Poller的ChannelMap和epoll监听对应的部分删除
  acceptChannel_.remove();
}

void Acceptor::listen() {
  listenning_ = true;
  acceptSocket_.listen();
  acceptChannel_.enableReading();
}

void Acceptor::handleRead() {
  InetAddress peerAddr;
  int connfd = acceptSocket_.accept(&peerAddr);
  if (connfd >= 0) {
    if (NewConnectionCallback_) {
      // tcpserver注册的回调,来分发连接
      NewConnectionCallback_(connfd, peerAddr);
    } else {
      ::close(connfd);
    }
  } else {
    LOG_FATAL << "accept err:" << ERR_MSG;
    if (errno == EMFILE) {
      LOG_FATAL << "sockfd reached limit";
    }
  }
}
}  // namespace net
}  // namespace gdrpc