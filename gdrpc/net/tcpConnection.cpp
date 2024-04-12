#include "tcpConnection.hpp"

#include <errno.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <functional>
#include <string>

#include "../util/log.hpp"
#include "channel.hpp"
#include "eventLoop.hpp"
#include "socket.hpp"
namespace gdrpc {
namespace net {

const char* TcpConnection::get_state() const {
  switch (state_) {
    case kDisconnected:
      return "kDisconnected";
    case kConnecting:
      return "kConnecting";
    case kConnected:
      return "kConnected";
    case kDisconnecting:
      return "kDisconnecting";
    default:
      return "unknown state";
  }
}

static EventLoop* CheckLoopNotNull(EventLoop* loop) {
  if (loop == nullptr) {
    LOG_FATAL << " mainLoop is null!";
  }
  return loop;
}

TcpConnection::TcpConnection(EventLoop* loop, const std::string& nameArg,
                             int sockfd, const InetAddress& localAddr,
                             const InetAddress& peerAddr)
    : loop_(CheckLoopNotNull(loop)),
      name_(nameArg),
      state_(kConnecting),
      reading_(true),
      channel_(new Channel(loop, sockfd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr),
      highWaterMark_(64 * 1024 * 1024)  // 64M
{
  channel_->setReadCallback([&](util::Timestamp ts) { handleRead(ts); });
  channel_->setWriteCallback([&]() { handleWrite(); });
  channel_->setCloseCallback([&]() { handleClose(); });
  channel_->setErrorCallback([&]() { handleError(); });

  LOG_INFO << "TcpConnection::ctor[" << name_ << "] at fd=" << sockfd;
  channel_->setFdOpt([&](int sockfd) {
    int flag = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag));
  });
}
TcpConnection::TcpConnection(EventLoop* loop, const std::string& nameArg,
                             std::unique_ptr<Channel>&& channel,
                             const InetAddress& localAddr,
                             const InetAddress& peerAddr)
    : loop_(CheckLoopNotNull(loop)),
      name_(nameArg),
      state_(kConnecting),
      reading_(true),
      channel_(std::move(channel)),
      localAddr_(localAddr),
      peerAddr_(peerAddr),
      highWaterMark_(64 * 1024 * 1024)  // 64M
{
  channel_->setReadCallback([&](util::Timestamp ts) { handleRead(ts); });
  channel_->setWriteCallback([&]() { handleWrite(); });
  channel_->setCloseCallback([&]() { handleClose(); });
  channel_->setErrorCallback([&]() { handleError(); });

  LOG_INFO << "TcpConnection::ctor[" << name_ << "] at fd=" << channel_->fd();
  channel_->setFdOpt([&](int sockfd) {
    int flag = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag));
  });
}

TcpConnection::~TcpConnection() {
  LOG_INFO << "TcpConnection::dtor[" << name_ << "] at fd=" << channel_->fd()
           << " state=" << get_state();
}

void TcpConnection::send(const std::string& buf) {
  if (state_ == kConnected) {
    if (loop_->isInLoopThread()) {
      LOG_DEBUG << "TcpConnection::dirctly send";
      sendInLoop(buf);
    } else {
      // std::string buf_(buf);
      // 要复制出一个对象，不然等异步执行的时候传进来的参数早就更改了，跟那go的老问题似的。
      // 反而不是这的问题，bind默认传递值,要传递引用的话要显式std::ref(buf)
      // https://stackoverflow.com/questions/31810985/why-does-bind-not-work-with-pass-by-reference/31811096#31811096
      LOG_DEBUG << "thread[" << CurrentThread::tid()
                << "]TcpConnection::send to thread[" << loop_->threadId()
                << "]";
      loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, shared_from_this(),
                                 buf));  // 我认为需要share_ptr
    }
  }
}

// void TcpConnection::sendInLoop(const void* data, size_t len) {
// 这样不行，data不知为何一直指向同一个地址，里面缓冲区的内容早就变了
void TcpConnection::sendInLoop(const std::string& s) {
  ssize_t len = s.length();
  const void* data = s.c_str();
  ssize_t nwrote = 0;
  size_t remaining = len;
  bool faultError = false;

  if (state_ == kDisconnected) {
    LOG_WARN << "TcpConnection " << name_ << ":: disconnected, give up writing";
    return;
  }

  // 表示channel_第一次开始写数据或者缓冲区没有待发送数据
  if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
    nwrote = ::write(channel_->fd(), data, len);
    if (nwrote >= 0) {
      remaining = len - nwrote;
      if (remaining == 0 && writeCompleteCallback_) {
        // 既然在这里数据全部发送完成，就不用在下面给channel设置epollout事件了
        loop_->queueInLoop(
            std::bind(writeCompleteCallback_, shared_from_this()));
      }
    } else  // nwrote < 0
    {
      nwrote = 0;
      if (errno != EWOULDBLOCK) {
        LOG_FATAL << "TcpConnection " << name_ << "::sendInLoop" << ERR_MSG();
        if (errno == EPIPE || errno == ECONNRESET) {
          faultError = true;
        }
      }
    }
  }
  if (!faultError && remaining > 0) {
    // 剩余长度
    size_t oldLen = outputBuffer_.readableBytes();
    // 积压待发送数据过多
    if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ &&
        highWaterMarkCallback_) {
      loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(),
                                   oldLen + remaining));
    }
    outputBuffer_.append((char*)data + nwrote, remaining);
    // 注册写事件
    if (!channel_->isWriting()) {
      channel_->enableWriting();
    }
  }
}

void TcpConnection::shutdown() {
  StateE s = kConnected;
  if (state_.compare_exchange_strong(s, kDisconnecting)) {
    loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
  }
}

void TcpConnection::shutdownInLoop() {
  if (!channel_->isWriting())  // 说明当前outputBuffer_的数据全部向外发送完成
  {
    channel_->shutdownWrite();
  }
  // 会在handlewrite发完后再调用该函数，所以此时未写完直接忽略
}

void TcpConnection::connectEstablished() {
  setState(kConnected);
  channel_->tie(shared_from_this());
  channel_->enableReading();  // 注册读事件

  connectionCallback_(shared_from_this());
}
void TcpConnection::connectDestroyed() {
  std::array<StateE, 2> s = {kConnected, kDisconnecting};
  for (auto state : s) {
    if (state_.compare_exchange_strong(state, kDisconnected)) {
      connectionCallback_(shared_from_this());
      break;
    }  // 不通过epollhub回调断连时的情况
  }
  channel_->remove();
}
// 注册给channel
void TcpConnection::handleRead(util::Timestamp receiveTime) {
  int savedErrno = 0;
  ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
  if (n > 0) {
    // 已建立连接的用户有可读事件发生了 调用用户传入的回调操作onMessage
    messageCallback_(shared_from_this(), receiveTime);
  } else if (n == 0)  // 客户端断开
  {
    handleClose();
  } else {
    errno = savedErrno;
    LOG_FATAL << "TcpConnection" << name_ << "::handleRead";
    handleError();
  }
}

void TcpConnection::handleWrite() {
  if (channel_->isWriting()) {
    int savedErrno = 0;
    ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
    if (n > 0) {
      outputBuffer_.retrieve(n);
      if (outputBuffer_.readableBytes() == 0) {
        channel_->disableWriting();
        if (writeCompleteCallback_) {
          // 我觉得可以直接执行回调，等着这个FATAL打我脸,应该只是调用时机的区别
          if (!loop_->isInLoopThread()) {
            LOG_FATAL
                << "TcpConnection " << name_
                << "::执行回调handleWrite的线程与TcpConnection所在线程不同";
          }
          loop_->queueInLoop(
              std::bind(writeCompleteCallback_, shared_from_this()));
        }
        if (state_ == kDisconnecting) {
          shutdownInLoop();
        }
      }
    } else {
      LOG_FATAL << "TcpConnection " << name_ << "::handleWrite";
    }
  } else {
    LOG_FATAL << "TcpConnection  " << name_ << "::fd=" << channel_->fd()
              << " is down, no more writing";
  }
}

void TcpConnection::handleClose() {
  LOG_INFO << "TcpConnection " << name_ << "::handleClose fd=" << channel_->fd()
           << " state=" << get_state();
  setState(kDisconnected);
  channel_->disableAll();

  TcpConnectionPtr connPtr(shared_from_this());
  connectionCallback_(connPtr);
  closeCallback_(connPtr);  // 执行的是TcpServer::removeConnection回调方法
}

void TcpConnection::handleError() {
  int optval;
  socklen_t optlen = sizeof optval;
  int err = 0;
  if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) ==
      0) {
    errno = optval;
  }
  LOG_FATAL << "TcpConnection " << name_
            << "::handleError  - SO_ERROR:" << ERR_MSG();
}

void TcpConnection::forceClose() {
  StateE state = kConnected;
  if (state_.compare_exchange_strong(state, kDisconnecting)) {
    loop_->queueInLoop(
        std::bind(&TcpConnection::forceCloseInLoop, shared_from_this()));
  }
}

void TcpConnection::forceCloseInLoop() {
  if (state_ == kConnected || state_ == kDisconnecting) {
    handleClose();
  }
}

bool TcpConnection::Buffer::retrieve(size_t len) {
  if (len < readableBytes()) {
    readerIndex_ += len;
  } else if (len == readableBytes()) {
    // 读完了，把地址刷到0
    retrieveAll();
  } else {
    return false;
  }
  return true;
}

ssize_t TcpConnection::Buffer::readFd(int fd, int* saveErrno) {
  // 栈额外空间,当buffer_暂时不够用时暂存数据，待buffer_重新分配足够空间后，在把数据交换给buffer_。
  char extrabuf[65536] = {0};

  struct iovec vec[2];
  const size_t writable = writableBytes();

  vec[0].iov_base = begin() + writerIndex_;
  vec[0].iov_len = writable;

  vec[1].iov_base = extrabuf;
  vec[1].iov_len = sizeof(extrabuf);

  const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
  const ssize_t n = ::readv(fd, vec, iovcnt);

  if (n < 0) {
    *saveErrno = errno;
  } else if (n <= writable) {  // Buffer够存
    writerIndex_ += n;
  } else {  // extrabuf里面也写入了数据
    writerIndex_ = buffer_.size();
    append(extrabuf, n - writable);
  }
  return n;
}

ssize_t TcpConnection::Buffer::writeFd(int fd, int* saveErrno) {
  ssize_t n = ::write(fd, peek(), readableBytes());
  if (n < 0) {
    *saveErrno = errno;
  }
  return n;
}

void TcpConnection::Buffer::makeSpace(size_t len) {
  // 放不下 扩容
  if (writableBytes() + prependableBytes() < len) {
    buffer_.resize(writerIndex_ + len);
  } else {
    // 移动整合空间
    size_t readable = readableBytes();
    std::copy(begin() + readerIndex_, begin() + writerIndex_, begin());
    readerIndex_ = 0;
    writerIndex_ = readable;
  }
}
}  // namespace net
}  // namespace gdrpc