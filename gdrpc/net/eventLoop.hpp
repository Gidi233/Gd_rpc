#ifndef _EVENTLOOP_
#define _EVENTLOOP_
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "../util/current_thread.hpp"
#include "../util/noncopyable.hpp"
#include "../util/timestamp.hpp"
#include "../util/timingWheel.hpp"
#include "channel.hpp"
#include "epoll_poller.hpp"
namespace gdrpc {
namespace net {
class Channel;
class EPollPoller;
#define TIMINGWHEEL_LEN 60 * 1000
// 事件循环类 主要包含了两个大模块 Channel EPollPoller
class EventLoop : noncopyable {
 public:
  using Functor = std::function<void()>;

  EventLoop();
  ~EventLoop();

  void loop();
  void quit();

  util::Timestamp pollReturnTime() const { return epoll_reture_time_; }

  // 在对应loop中执行回调
  void runInLoop(Functor cb);
  void queueInLoop(Functor cb);
  void runAfter(int64_t ms, Functor cb);

  // 通过eventfd唤醒loop所在的线程
  void wakeup();

  void updateChannel(Channel* channel);
  void removeChannel(Channel* channel);
  bool hasChannel(Channel* channel);

  // 判断EventLoop对象是否在自己的线程里
  bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

  pid_t threadId() { return threadId_; }

 private:
  using ChannelList = std::vector<Channel*>;
  void handleRead(util::Timestamp ts);  // 被其他线程唤醒时读wakeupFd_的8字节
  void doPendingFunctors();             // 执行上层回调
  void doScheduledFunctors(util::Timestamp& ts);
  std::atomic_bool looping_;
  std::atomic_bool quit_;

  const pid_t threadId_;  // 标识了当前EventLoop的所属线程id

  util::Timestamp epoll_reture_time_;  // Poller返回发生事件的Channels的时间点
  std::unique_ptr<EPollPoller> poller_;

  int wakeupFd_;  // 用于被其他线程唤醒
  std::unique_ptr<Channel> wakeupChannel_;

  util::TimeWheel timeWheel_;
  ChannelList active_channels_;
  std::atomic_bool calling_pending_functors_;  // 是否有需要执行的回调操作
  std::vector<Functor> pending_functors_;  // 存储loop需要执行的所有回调操作
  std::mutex mutex_;
};
}  // namespace net
}  // namespace gdrpc
#endif