#include <iostream>
#include <thread>

#include "../gdrpc/util/log.hpp"

void fun() {
  int i = 20;
  while (i--) {
    LOG_DEBUG << "debug this is thread in ";
    LOG_INFO << "info this is thread in fun";
  }
}

int main() {
  // gdlog::Logger::InitGlobalLogger();
  gdlog::Logger::setLogLevel(gdlog::Logger::DEBUG);

  // 创建并启动线程
  std::thread t(fun);

  // 主线程中的循环
  int i = 20;
  while (i--) {
    LOG_DEBUG << "debug this is the main thread";
    LOG_INFO << "info this is the main thread";
  }

  // 等待子线程结束
  t.join();

  return 0;
}