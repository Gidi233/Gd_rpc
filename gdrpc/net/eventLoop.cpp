#include "eventLoop.hpp"

#include <errno.h>
#include <fcntl.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <cstring>
#include <memory>

#include "../util/log.hpp"
#include "channel.hpp"
#include "epoll_poller.hpp"
namespace gdrpc {
namespace net {
// 防止一个线程创建多个EventLoop
__thread EventLoop* t_loopInThisThread = nullptr;

// 超时时间
const int kPollTimeMs = 1;

int createEventfd() {
  int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (evtfd < 0) {
    LOG_FATAL << "eventfd error: " << ERR_MSG();
  }
  return evtfd;
}

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      calling_pending_functors_(false),
      threadId_(CurrentThread::tid()),
      poller_(new EPollPoller(this)),
      wakeupFd_(createEventfd()),
      wakeupChannel_(new Channel(this, wakeupFd_)),
      timeWheel_(TIMINGWHEEL_LEN) {
  LOG_DEBUG << "EventLoop created " << this << " in thread " << threadId_;
  if (t_loopInThisThread) {
    LOG_FATAL << "Another EventLoop " << t_loopInThisThread
              << " exists in this thread " << threadId_;
  } else {
    t_loopInThisThread = this;
  }
  wakeupChannel_->setReadCallback(
      [this](util::Timestamp ts) { handleRead(ts); });
  wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
  wakeupChannel_->disableAll();
  wakeupChannel_->remove();
  ::close(wakeupFd_);
  t_loopInThisThread = nullptr;
}

// 开启事件循环
void EventLoop::loop() {
  looping_ = true;
  quit_ = false;

  LOG_DEBUG << "EventLoop " << this << " start looping.";

  while (!quit_) {
    active_channels_.clear();
    epoll_reture_time_ = poller_->poll(kPollTimeMs, active_channels_);
    doScheduledFunctors(epoll_reture_time_);
    for (Channel* channel : active_channels_) {
      channel->handleEvent(epoll_reture_time_);
    }
    doPendingFunctors();
  }
  LOG_DEBUG << "EventLoop " << this << " stop looping.";
  looping_ = false;
}

void EventLoop::quit() {
  quit_ = true;
  if (!isInLoopThread()) {
    wakeup();
  }
}

void EventLoop::runInLoop(Functor cb) {
  if (isInLoopThread())  // 在当前EventLoop
  {
    cb();
  } else  // 非当前EventLoop
  {
    queueInLoop(cb);
  }
}

// 把cb放入队列中 唤醒loop所在的线程执行cb
void EventLoop::queueInLoop(Functor cb) {
  {
    std::unique_lock<std::mutex> lock(mutex_);
    pending_functors_.emplace_back(cb);
  }
  // || callingPendingFunctors的意思是 当前loop正在执行回调中
  // 但是loop的pendingFunctors_中又加入了新的回调 需要通过wakeup写事件
  // 唤醒相应的需要执行上面回调操作的loop的线程
  // 让loop()下一次poller_->poll()不再阻塞（因为有待办事件）
  if (!isInLoopThread() || calling_pending_functors_) {
    wakeup();
  }
}

void EventLoop::runAfter(int64_t ms, Functor cb) { timeWheel_.addTask(ms, cb); }

void EventLoop::handleRead(util::Timestamp ts) {
  uint64_t one = 1;
  ssize_t n = read(wakeupFd_, &one, sizeof(one));
  if (n != sizeof(one)) {
    LOG_WARN << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
  }
}

// epoll读事件用来唤醒loop所在线程
void EventLoop::wakeup() {
  uint64_t one = 1;
  ssize_t n = write(wakeupFd_, &one, sizeof(one));
  if (n != sizeof(one)) {
    LOG_WARN << "EventLoop::wakeup() writes " << n << " bytes instead of 8\n";
  }
}

void EventLoop::updateChannel(Channel* channel) {
  poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel) {
  poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel* channel) {
  return poller_->hasChannel(channel);
}

void EventLoop::doPendingFunctors() {
  std::vector<Functor> functors;
  calling_pending_functors_ = true;

  {
    std::unique_lock<std::mutex> lock(mutex_);
    functors.swap(pending_functors_);
  }

  for (const Functor& functor : functors) {
    functor();
  }

  calling_pending_functors_ = false;
}

void EventLoop::doScheduledFunctors(util::Timestamp& ts) {
  timeWheel_.takeAllTimeout(ts);
}
}  // namespace net
}  // namespace gdrpc