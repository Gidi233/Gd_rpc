#include "timingWheel.hpp"

#include <iostream>

#include "timestamp.hpp"
namespace gdrpc {
namespace util {

TimeWheel::TimeWheel(int msLen)
    : slotLen(msLen),
      currentSlot_(0),
      slots_(slotLen),
      starttime_(Timestamp().get_ms()) {}

std::optional<std::weak_ptr<Task>> TimeWheel::addTask(
    int64_t timeout, std::function<void(void)> fun) {
  if (timeout < 0) {
    fun();
    return std::nullopt;
  } else {
    auto slot = (currentSlot_ + (timeout % slotLen)) % slotLen;
    auto task = std::make_shared<Task>(timeout / slotLen, timeout, fun);
    slots_[slot].push_back(task);
    return std::weak_ptr<Task>(task);
  }
}

// std::future
// auto TimeWheel::addTaskFromOtherThread(int64_t originTime,int timeout
//                                        std::function<void(void)> fun,
//                                        void* args) {
//   return addTask(timeout, fun, args);
// }

void TimeWheel::delTask(std::weak_ptr<Task> task) {
  auto taskPtr = task.lock();
  if (taskPtr) taskPtr->remove();
}

void TimeWheel::tick() {
  for (auto it = slots_[currentSlot_].begin();
       it != slots_[currentSlot_].end();) {
    if ((*it)->getRotations() > 0) {
      (*it)->decreaseRotations();
      ++it;
      continue;
    } else {
      auto task = *it;
      task->exec();
      auto times = task->getTimes();
      if (times > 1 || times == -1) {
        if (times > 1) {
          task->decreaseTimes();
        }
        auto timeout = task->getTimeout();
        task->setRotations(timeout / slotLen);
        auto slot = (currentSlot_ + (timeout % slotLen)) % slotLen;
        slots_[slot].push_back(task);
      }
      it = slots_[currentSlot_].erase(it);
    }
  }

  currentSlot_ = ++currentSlot_ % slotLen;
}

void TimeWheel::takeAllTimeout(Timestamp& ts) {
  auto now = ts.get_ms();
  auto cnt = now - starttime_;
  for (auto i = 0; i < cnt; ++i) tick();
  starttime_ = now;
}

}  // namespace util
}  // namespace gdrpc
