#ifndef _CURRENT_THREAD_
#define _CURRENT_THREAD_

#include <sys/syscall.h>
#include <unistd.h>

namespace CurrentThread {
// __thread 每个线程独立
// 保存tid缓存 因为系统调用非常耗时 拿到tid后将其保存
extern __thread int t_cachedTid;

void cacheTid();

int tid();

}  // namespace CurrentThread

#endif