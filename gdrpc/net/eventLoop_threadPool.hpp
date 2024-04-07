#ifndef _EVENTLOOP_THREADPOOL_
#define _EVENTLOOP_THREADPOOL_
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../util/noncopyable.hpp"

namespace gdrpc {
namespace net {
class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable {
 public:
  using ThreadInitCallback = std::function<void(EventLoop*)>;
  using SchedulingFunction = std::function<EventLoop*()>;

  EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg);
  ~EventLoopThreadPool();

  void setThreadNum(int numThreads) { numThreads_ = numThreads; }

  void start(const ThreadInitCallback& cb = ThreadInitCallback());

  void setSchedulingFunction(SchedulingFunction func) { getNextLoop = func; }

  // 如果工作在多线程中，baseLoop_(mainLoop)会默认以轮询的方式分配Channel给subLoop
  SchedulingFunction getNextLoop = [&]() {
    // 如果只设置一个线程 也就是只有一个mainReactor 无subReactor
    // 那么每次都返回当前的baseLoop_
    EventLoop* loop = baseLoop_;
    if (!loops_.empty())  // 通过轮询获取下一个处理事件的loop
    {
      loop = loops_[next_];
      ++next_;
      if (next_ >= loops_.size()) {
        next_ = 0;
      }
    }
    return loop;
  };

  std::vector<EventLoop*> getAllLoops();

  bool started() const { return started_; }
  const std::string& name() const { return name_; }

 private:
  // 用户使用muduo创建的loop 如果线程数为1 那直接使用用户创建的loop
  EventLoop* baseLoop_;
  const std::string name_;
  bool started_;
  int numThreads_;
  int next_;  // 轮询的下标
  std::vector<std::unique_ptr<EventLoopThread>> threads_;
  std::vector<EventLoop*> loops_;
};
}  // namespace net
}  // namespace gdrpc
#endif