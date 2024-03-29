#ifndef _EVENTLOOP_THREAD_
#define _EVENTLOOP_THREAD_
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "../util/noncopyable.hpp"
namespace gdrpc {
namespace net {
class EventLoop;

class EventLoopThread : noncopyable {
 public:
  using ThreadInitCallback = std::function<void(EventLoop*)>;

  explicit EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(),
                           const std::string& name = std::string());
  ~EventLoopThread();

  EventLoop* startLoop();

 private:
  void threadFunc();

  EventLoop* loop_;
  bool exiting_;
  std::thread thread_;
  std::mutex mutex_;
  std::condition_variable cond_;
  ThreadInitCallback callback_;
};
}  // namespace net
}  // namespace gdrpc
#endif