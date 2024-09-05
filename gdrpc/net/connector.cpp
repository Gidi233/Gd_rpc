#include "connector.hpp"

#include <errno.h>

#include "../util/log.hpp"
#include "channel.hpp"
#include "eventLoop.hpp"

namespace gdrpc::net {
namespace {
auto getSocketError = [](int sockfd) {
  int optval;
  socklen_t optlen = static_cast<socklen_t>(sizeof optval);
  if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
    return errno;
  } else {
    return optval;
  }
};
}  // namespace

Connector::Connector(EventLoop* loop, const InetAddress& serverAddr)
    : loop_(loop),
      serverAddr_(serverAddr),
      connect_(false),
      state_(kDisconnected),
      retryDelayMs_(kInitRetryDelayMs) {
  LOG_DEBUG << "Connector ctor[" << this << "]";
}

Connector::~Connector() { LOG_DEBUG << "dtor[" << this << "]"; }

void Connector::start() {
  connect_ = true;
  loop_->runInLoop([this] { startInLoop(); });
}

void Connector::startInLoop() {
  if (connect_) {
    LOG_DEBUG << "start connect";
    connect();
  } else {
    LOG_DEBUG << "do not connect";
  }
}

void Connector::stop() {
  connect_ = false;
  loop_->queueInLoop(std::bind(&Connector::stopInLoop, this));
}

void Connector::stopInLoop() {
  States s = kConnecting;
  if (state_.compare_exchange_strong(s, kDisconnected)) {
    int sockfd = removeAndResetChannel();
    retry(sockfd);
  }
}

void Connector::connect() {
  int sockfd =
      ::socket(serverAddr_.family(), SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
               IPPROTO_TCP);
  if (sockfd < 0) {
    LOG_FATAL << "sockets::createNonblockingOrDie";
  }
  int ret = ::connect(
      sockfd, reinterpret_cast<const sockaddr*>(serverAddr_.getSockAddr()),
      static_cast<socklen_t>(sizeof(struct sockaddr_in6)));
  int savedErrno = (ret == 0) ? 0 : errno;
  LOG_DEBUG << "::connect() return " << ERR_MSG(savedErrno);
  switch (savedErrno) {
    case 0:
    case EINPROGRESS:
    case EINTR:
    case EISCONN:
      connecting(sockfd);
      break;

    case EAGAIN:
    case EADDRINUSE:
    case EADDRNOTAVAIL:
    case ECONNREFUSED:
    case ENETUNREACH:
      retry(sockfd);
      break;

    case EACCES:
    case EPERM:
    case EAFNOSUPPORT:
    case EALREADY:
    case EBADF:
    case EFAULT:
    case ENOTSOCK:
      LOG_FATAL << "connect error in Connector::startInLoop " << savedErrno;
      ::close(sockfd);
      break;

    default:
      LOG_FATAL << "Unexpected error in Connector::startInLoop " << savedErrno;
      ::close(sockfd);
      // connectErrorCallback_();
      break;
  }
}

void Connector::restart() {
  setState(kDisconnected);
  retryDelayMs_ = kInitRetryDelayMs;
  connect_ = true;
  startInLoop();
}

void Connector::connecting(int sockfd) {
  setState(kConnecting);
  channel_ = std::make_unique<Channel>(loop_, sockfd);
  channel_->setWriteCallback([this] { handleWrite(); });
  channel_->setErrorCallback([this] { handleError(); });
  channel_->enableWriting();
  LOG_DEBUG << "注册connecting";
}

int Connector::removeAndResetChannel() {
  channel_->disableAll();
  channel_->remove();
  int sockfd = channel_->fd();
  // 不能在此处重置channel_，现在在channel：：handleEvent内
  loop_->queueInLoop([this] { resetChannel(); });
  return sockfd;
}

void Connector::resetChannel() { channel_.reset(); }

// 连接建立完成时触发
void Connector::handleWrite() {
  LOG_DEBUG << "Connector::handleWrite " << state_;

  if (state_ == kConnecting) {
    int sockfd = channel_->fd();  //
    // int sockfd = removeAndResetChannel();
    int err = getSocketError(sockfd);
    if (err) {
      LOG_WARN << "Connector::handleWrite - SO_ERROR = " << ERR_MSG(err);
      removeAndResetChannel();
      retry(sockfd);
    } else {
      if (connect_) {
        channel_->disableAll();
        setState(kConnected);
        newConnectionCallback_(std::move(channel_));
      } else {
        removeAndResetChannel();
        close(sockfd);
      }
    }
  } else {
    LOG_FATAL << "unknown connector handlewrite";
  }
}

void Connector::handleError() {
  LOG_WARN << "Connector::handleError state=" << state_;
  if (state_ == kConnecting) {
    int sockfd = removeAndResetChannel();
    int err = getSocketError(sockfd);
    LOG_DEBUG << "SO_ERROR = " << err << " " << ERR_MSG(err);
    retry(sockfd);
  }
}

void Connector::retry(int sockfd) {
  LOG_DEBUG << "Connector kDisconnected";
  ::close(sockfd);
  setState(kDisconnected);
  if (connect_) {
    LOG_INFO << "Connector::retry - Retry connecting to "
             << serverAddr_.toIpPort() << " in " << retryDelayMs_
             << " milliseconds. ";
    loop_->queueInLoop(std::bind(&Connector::startInLoop, shared_from_this()));
    loop_->runAfter(retryDelayMs_,
                    std::bind(&Connector::startInLoop, shared_from_this()));
    retryDelayMs_ = std::min(retryDelayMs_ * 2, kMaxRetryDelayMs);
  } else {
    LOG_DEBUG << "do not connect";
  }
}
}  // namespace gdrpc::net