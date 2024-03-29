#ifndef _CHANNEL_
#define _CHANNEL_
#include <sys/epoll.h>

#include <functional>
#include <memory>

#include "../util/noncopyable.hpp"
#include "../util/timestamp.hpp"
namespace gdrpc {
namespace net {
class EventLoop;
enum class ChannelState;
// Channel理解为通道 封装了sockfd和其感兴趣的event
class Channel : noncopyable {
 public:
  using EventCallback = std::function<void()>;
  using ReadEventCallback = std::function<void(util::Timestamp)>;

  Channel(EventLoop* loop, int fd);
  ~Channel();

  // fd得到Poller通知以后 处理事件 handleEvent在EventLoop::loop()中调用
  void handleEvent(util::Timestamp receiveTime);

  void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
  void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
  void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
  void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

  // 防止当channel被手动remove掉 channel还在执行回调操作
  void tie(const std::shared_ptr<void>&);

  int fd() const { return fd_; }
  int events() const { return events_; }
  void set_revents(int revt) { return_events_ = revt; }

  // 设置fd相应的事件状态 相当于epoll_ctl add delete
  void enableReading() {
    events_ |= kReadEvent;
    update();
  }
  void disableReading() {
    events_ &= ~kReadEvent;
    update();
  }
  void enableWriting() {
    events_ |= kWriteEvent;
    update();
  }
  void disableWriting() {
    events_ &= ~kWriteEvent;
    update();
  }
  void disableAll() {
    events_ = kNoneEvent;
    update();
  }

  // 返回fd当前的事件状态
  bool isNoneEvent() const { return events_ == kNoneEvent; }
  bool isWriting() const { return events_ & kWriteEvent; }
  bool isReading() const { return events_ & kReadEvent; }

  ChannelState state() { return state_; }
  void set_state(ChannelState state) { state_ = state; }

  // one loop per thread
  EventLoop* ownerLoop() { return loop_; }
  void remove();

 private:
  void update();
  void handleEventWithGuard(util::Timestamp receiveTime);

  static constexpr int kNoneEvent = 0;
  static constexpr int kReadEvent = EPOLLIN | EPOLLPRI;
  static constexpr int kWriteEvent = EPOLLOUT;

  EventLoop* loop_;  // 所属循环
  const int fd_;
  int events_;         // 注册fd感兴趣的事件
  int return_events_;  // epoll返回的具体发生的事件
  ChannelState state_;  // 在epoll中的状态

  // 因为资源可能已经消失，用的时候再获取
  std::weak_ptr<void> tie_;
  bool tied_;

  // 具体事件的回调操作
  ReadEventCallback readCallback_;
  EventCallback writeCallback_;
  EventCallback closeCallback_;
  EventCallback errorCallback_;
};
}  // namespace net
}  // namespace gdrpc
#endif