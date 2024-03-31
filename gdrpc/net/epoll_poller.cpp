#include "epoll_poller.hpp"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "channel.hpp"
#include "eventLoop.hpp"

namespace gdrpc {
namespace net {

namespace {
inline const char* ToString(ChannelState state) {
  switch (state) {
    case ChannelState::kNew:
      return "kNew";
    case ChannelState::kAdded:
      return "kAdded";
    case ChannelState::kDeleted:
      return "kDeleted";
    default:
      return "Unknown State";
  }
}
}  // namespace

EPollPoller::EPollPoller(EventLoop* loop)
    : loop_(loop),
      epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize)  // vector<epoll_event>(16)
{
  if (epollfd_ < 0) {
    LOG_FATAL << "epoll_create error:" << ERR_MSG;
  }
}

EPollPoller::~EPollPoller() { ::close(epollfd_); }

util::Timestamp EPollPoller::poll(int timeoutMs, ChannelList& activeChannels) {
  LOG_DEBUG << "fd total count:" << channels_.size();

  int numEvents = ::epoll_wait(epollfd_, &*events_.begin(),
                               static_cast<int>(events_.size()), timeoutMs);
  int saveErrno = errno;
  util::Timestamp now;

  if (numEvents > 0) {
    LOG_DEBUG << numEvents << " events happend";
    fillActiveChannels(numEvents, activeChannels);
    if (numEvents == events_.size())  // 扩容操作
    {
      events_.resize(events_.size() * 2);
    }
  } else if (numEvents == 0) {
    LOG_DEBUG << "timeout!";
  } else {
    if (saveErrno != EINTR) {
      errno = saveErrno;
      LOG_FATAL << "EPollPoller::poll() error!";
    }
  }
  return now;
}

// channel update  => EventLoop updateChannel  => Poller updateChannel
void EPollPoller::updateChannel(Channel* channel) {
  const ChannelState state = channel->state();
  LOG_DEBUG << "fd=" << channel->fd() << " events=" << channel->events()
            << " state=" << ToString(state);

  // 用户读完了可以取消监听读事件，channel状态变为kDeleted，但连接还没关闭，用户还可以发送数据，发送缓冲区满，就会重新kADDed,注册写事件
  if (state == ChannelState::kNew || state == ChannelState::kDeleted) {
    if (state == ChannelState::kNew) {
      int fd = channel->fd();
      channels_[fd] = channel;
    }
    channel->set_state(ChannelState::kAdded);
    update(EPOLL_CTL_ADD, channel);
  } else {
    int fd = channel->fd();
    if (channel->isNoneEvent()) {
      update(EPOLL_CTL_DEL, channel);
      channel->set_state(ChannelState::kDeleted);
    } else {
      update(EPOLL_CTL_MOD, channel);
    }
  }
}

void EPollPoller::removeChannel(Channel* channel) {
  int fd = channel->fd();
  channels_.erase(fd);

  LOG_DEBUG << "remove:fd=" << fd;

  ChannelState state = channel->state();
  if (state == ChannelState::kAdded) {
    update(EPOLL_CTL_DEL, channel);
  }
  //   channel->set_state(kNew);
  // 之后就随着tcpconnect释放了 不会复用
}
bool EPollPoller::hasChannel(Channel* channel) const {
  auto it = channels_.find(channel->fd());
  return it != channels_.end() && it->second == channel;
}

// 填写活跃的连接
void EPollPoller::fillActiveChannels(int numEvents,
                                     ChannelList& activeChannels) const {
  for (int i = 0; i < numEvents; ++i) {
    Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
    channel->set_revents(events_[i].events);
    activeChannels.push_back(channel);
  }
}

// 更新channel通道 其实就是调用epoll_ctl add/mod/del
void EPollPoller::update(int operation, Channel* channel) {
  epoll_event event;
  ::memset(&event, 0, sizeof(event));

  int fd = channel->fd();

  event.events = channel->events();
  event.data.fd = fd;
  event.data.ptr = channel;

  if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
    if (operation == EPOLL_CTL_DEL) {
      LOG_FATAL << "epoll_ctl del error:" << ERR_MSG;
    } else {
      LOG_FATAL << "epoll_ctl add/mod error:" << ERR_MSG;
    }
  }
}
}  // namespace net
}  // namespace gdrpc