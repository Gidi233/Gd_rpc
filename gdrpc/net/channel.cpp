#include "channel.hpp"

#include <sys/socket.h>

#include "../util/log.hpp"
#include "epoll_poller.hpp"
#include "eventLoop.hpp"
namespace gdrpc {
namespace net {
Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop),
      fd_(fd),
      events_(0),
      return_events_(0),
      state_(ChannelState::kNew),
      tied_(false) {}

Channel::~Channel() { ::close(fd_); }

void Channel::tie(const std::shared_ptr<void>& obj) {
  tie_ = obj;
  tied_ = true;
}

// channel->eventloop->epoll
// unique_ptr本就可以get()获得原指针，这里把指针传出去没有破坏了unique的语意
void Channel::update() { loop_->updateChannel(this); }
void Channel::remove() { loop_->removeChannel(this); }

void Channel::handleEvent(util::Timestamp receiveTime) {
  if (tied_) {
    // 如果活着就调用
    std::shared_ptr<void> guard = tie_.lock();
    if (guard) {
      handleEventWithGuard(receiveTime);
    }
    // 否则忽略，说明Channel的TcpConnection对象已经不存在了
  } else {
    handleEventWithGuard(receiveTime);
  }
}

void Channel::handleEventWithGuard(util::Timestamp receiveTime) {
  LOG_DEBUG << "channel handleEvent revents:" << return_events_;
  // 关闭
  if ((return_events_ & EPOLLHUP) && !(return_events_ & EPOLLIN)) {
    if (closeCallback_) {
      closeCallback_();
    }
  }
  // 错误
  if (return_events_ & EPOLLERR) {
    if (errorCallback_) {
      errorCallback_();
    }
  }
  // 读
  if (return_events_ & (kReadEvent)) {
    if (readCallback_) {
      readCallback_(receiveTime);
    }
  }
  // 写
  if (return_events_ & kWriteEvent) {
    if (writeCallback_) {
      writeCallback_();
    }
  }
}

void Channel::shutdownWrite() {
  if (::shutdown(fd_, SHUT_WR) < 0) {
    LOG_FATAL << "shutdownWrite error";
  }
}
}  // namespace net
}  // namespace gdrpc