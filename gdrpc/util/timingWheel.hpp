#ifndef _TIMING_WHEEL_
#define _TIMING_WHEEL_

#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "timestamp.hpp"
namespace gdrpc {
namespace util {
class Task {
 public:
  Task(int rotations, int timeout, std::function<void(void)> fun)
      : rotations_(rotations), timeout_(timeout), fun(fun) {}

  Task(int rotations, int timeout, std::function<void(void)> fun, int times)
      : rotations_(rotations), timeout_(timeout), fun(fun), times_(times) {}
  inline int getRotations() { return rotations_; }
  inline void setRotations(int rotations) { rotations_ = rotations; }
  inline void decreaseRotations() { --rotations_; }

  inline int getTimes() { return times_; }
  inline void decreaseTimes() { --times_; }
  inline int getTimeout() { return timeout_; }
  inline void exec() {
    if (fun) fun();
  }
  inline void remove() { fun = nullptr; }

  // inline int getSlot() { return slot_; }

 private:
  int rotations_;
  // int slot_;
  int timeout_;
  int times_;

  std::function<void(void)> fun;
};

class TimeWheel {
 public:
  explicit TimeWheel(int msLen);
  ~TimeWheel() {}

  std::optional<std::weak_ptr<Task>> addTask(int64_t timeout,
                                             std::function<void(void)> fun);
  //  TODO: refresh(std::weak_ptr<Task>);
  //   auto addTaskFromOtherThread(int64_t time, std::function<void(void)> fun,
  //    void* args);
  void delTask(std::weak_ptr<Task> task);
  void tick();
  void takeAllTimeout(Timestamp&);

 private:
  int slotLen;
  int currentSlot_;
  int64_t starttime_;
  std::vector<std::list<std::shared_ptr<Task>>> slots_;
};
}  // namespace util
}  // namespace gdrpc
#endif