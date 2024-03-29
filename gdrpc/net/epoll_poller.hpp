#ifndef _EPOLL_POLLER_
#define _EPOLL_POLLER_
#include <sys/epoll.h>

#include <vector>

#include "../util/log.hpp"
#include "../util/timestamp.hpp"
#include "channel.hpp"
#include "eventLoop.hpp"
namespace gdrpc {
namespace net {
enum class ChannelState {
  kNew,     // 还没添加至Poller
  kAdded,   // 已添加至Poller
  kDeleted  // 已从Poller删除
};

// 不知道为什么光包头文件还不行还要预声明
class Channel;
class EventLoop;

class EPollPoller {
 public:
  EPollPoller(EventLoop* loop);
  ~EPollPoller();

  using ChannelList = std::vector<Channel*>;
  util::Timestamp poll(int timeoutMs, ChannelList& activeChannels);
  void updateChannel(Channel* channel);
  void removeChannel(Channel* channel);
  bool hasChannel(Channel* channel) const;

 private:
  // sockfd->channel
  using ChannelMap = std::unordered_map<int, Channel*>;
  ChannelMap channels_;

  EventLoop* loop_;  // 定义Poller所属的事件循环EventLoop
  static const int kInitEventListSize = 16;

  // 填写活跃的连接
  void fillActiveChannels(int numEvents, ChannelList& activeChannels) const;
  void update(int operation, Channel* channel);

  using EventList = std::vector<epoll_event>;

  int epollfd_;
  EventList events_;  // 所有发生的事件的文件描述符事件集
};
}  // namespace net
}  // namespace gdrpc
#endif