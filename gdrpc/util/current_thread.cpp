#include "current_thread.hpp"

namespace CurrentThread {

__thread int t_cachedTid = 0;

void cacheTid() {
  if (t_cachedTid == 0) {
    t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
  }
}

int tid() {
  // __builtin_expect 是一种底层优化 此语句意思是如果还未获取tid
  // 进入if 通过cacheTid()系统调用获取tid
  if (__builtin_expect(t_cachedTid == 0, 0)) {
    cacheTid();
  }
  return t_cachedTid;
}

}  // namespace CurrentThread