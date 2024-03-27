#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "../gdrpc/util/async_log.hpp"
#include "../gdrpc/util/log.hpp"

#define THREAD_NUM 5
#define LOG_NUM 100000
#define kRollSize (20 * 1024 * 1024)

std::unique_ptr<gdrpc::gdlog::AsyncLogging> g_asyncLog;

void fun() {
  int i = LOG_NUM;
  while (i--) {
    LOG_DEBUG << "debug in fun ";
    LOG_INFO << "info in fun";
  }
}

void test() {
  auto start_time = std::chrono::system_clock::now();
  std::vector<std::thread> t(THREAD_NUM - 1);
  for (int i = 0; i < THREAD_NUM - 1; i++) t[i] = std::thread(fun);

  // 主线程中的循环
  int i = LOG_NUM;
  while (i--) {
    LOG_DEBUG << "debug in main ";
    LOG_INFO << "info in main ";
  }

  // 等待子线程结束
  for (int i = 0; i < THREAD_NUM - 1; i++) t[i].join();
  auto end_time = std::chrono::system_clock::now();
  auto duration = end_time - start_time;
  auto millisecs =
      std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
  std::cout << "Total time taken: " << millisecs << " milliseconds"
            << std::endl;
}
int main(int argc, char* argv[]) {
  // gdrpc::gdlog::Logger::InitGlobalLogger();
  gdrpc::gdlog::Logger::setLogLevel(gdrpc::gdlog::Logger::DEBUG);
  std::cout << "直接终端输出" << std::endl;
  test();

  g_asyncLog = std::make_unique<gdrpc::gdlog::AsyncLogging>(argv[0], kRollSize);
  gdrpc::gdlog::Logger::setOutput(
      [&](const char* msg, int len) { g_asyncLog->append(msg, len); });
  gdrpc::gdlog::Logger::setFlush([&]() { g_asyncLog->flush(); });
  std::cout << "异步输出到文件" << std::endl;
  test();
  return 0;
}