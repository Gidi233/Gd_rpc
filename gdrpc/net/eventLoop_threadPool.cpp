#include "eventLoop_threadPool.hpp"

#include <memory>

#include "eventLoop_thread.hpp"
namespace gdrpc {
namespace net {
EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop,
                                         const std::string& nameArg)
    : baseLoop_(baseLoop),
      name_(nameArg),
      started_(false),
      numThreads_(0),
      next_(0) {}

EventLoopThreadPool::~EventLoopThreadPool() {
  // Don't delete loop, it's stack variable
}

void EventLoopThreadPool::start(const ThreadInitCallback& cb) {
  started_ = true;

  for (int i = 0; i < numThreads_; ++i) {
    char buf[name_.size() + 32];
    snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
    EventLoopThread* t = new EventLoopThread(cb, buf);
    // 底层创建线程 绑定一个新的EventLoop 并返回该loop的地址
    threads_.push_back(std::unique_ptr<EventLoopThread>(t));
    loops_.push_back(t->startLoop());
  }
  // 整个服务端只有一个线程，就都运行在baseLoop上
  if (numThreads_ == 0 && cb) {
    cb(baseLoop_);
  }
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops() {
  if (loops_.empty()) {
    return std::vector<EventLoop*>(1, baseLoop_);
  } else {
    return loops_;
  }
}
}  // namespace net
}  // namespace gdrpc