#include "eventLoop_thread.hpp"

#include "eventLoop.hpp"

namespace gdrpc {
namespace net {
EventLoopThread::EventLoopThread(const ThreadInitCallback& cb,
                                 const std::string& name)
    : loop_(nullptr), exiting_(false), mutex_(), cond_(), callback_(cb) {}

EventLoopThread::~EventLoopThread() {
  exiting_ = true;
  if (loop_ != nullptr) {
    loop_->quit();
    thread_.join();
  }
}

EventLoop* EventLoopThread::startLoop() {
  thread_ = std::thread([&]() { threadFunc(); });
  LOG_DEBUG << "new EventLoopThread started";
  EventLoop* loop = nullptr;
  {  // 这里感觉都可以不用上锁，指针改变是原子的，自旋不了一会
    //  还是加上让语意完备吧，万一编译器给优化成while(1)了
    std::unique_lock<std::mutex> lock(mutex_);
    while (loop_ == nullptr) {
      cond_.wait(lock);
    }
    loop = loop_;
  }
  return loop;
}

void EventLoopThread::threadFunc() {
  // 创建一个独立的EventLoop对象和上面的线程是对应的 one loop per thread
  EventLoop loop;

  if (callback_) {
    callback_(&loop);
  }

  {
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = &loop;
    cond_.notify_one();
  }
  loop.loop();  // 开启了底层的epoll
  std::unique_lock<std::mutex> lock(mutex_);
  loop_ = nullptr;
}
}  // namespace net
}  // namespace gdrpc