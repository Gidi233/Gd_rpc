#include <iostream>
#include <random>
#include <utility>
#include <vector>

#include "../gdrpc/net/eventLoop.hpp"
#include "../gdrpc/util/async_log.hpp"
#include "../gdrpc/util/log.hpp"
#include "../gdrpc/util/timestamp.hpp"
#include "../gdrpc/util/timingWheel.hpp"

using namespace gdrpc::net;
using namespace std;
using namespace gdrpc::gdlog;
using namespace gdrpc::util;

#define kRollSize (20 * 1024 * 1024)
std::unique_ptr<gdrpc::gdlog::AsyncLogging> g_asyncLog;

class checkTask {
 public:
  explicit checkTask(const int64_t timeout)
      : timeout_(timeout), t_(Timestamp()) {}

  void check() const {
    auto t1 = t_.get_ms();
    auto t2 = Timestamp().get_ms();
    cout << "timeout: " << timeout_ << " ms "
         << "startTime: " << t1 << " triggerTime: " << t2
         << " Difference: " << t2 - t1 << endl;
  }
  int64_t timeout_;
  Timestamp t_;
};

void generateRandomNumbers(std::vector<int>& numbers, int count) {
  std::mt19937 gen(std::random_device{}());

  std::uniform_int_distribution<int> dist(1, 5000);

  for (int i = 0; i < count; ++i) {
    numbers[i] = dist(gen);
  }
}

int main(int argc, char* argv[]) {
  g_asyncLog = std::make_unique<AsyncLogging>(argv[0], kRollSize);
  gdrpc::gdlog::Logger::setOutput(
      [&](const char* msg, int len) { g_asyncLog->append(msg, len); });
  gdrpc::gdlog::Logger::setFlush([&]() { g_asyncLog->flush(); });
  // 生成100个1到5000之间的随机数
  int count = 100;
  std::vector<int> randomNumbers(count);
  generateRandomNumbers(randomNumbers, count);
  EventLoop loop;
  for (int i : randomNumbers) {
    loop.runAfter(i, [task = checkTask(i)]() { task.check(); });
  }
  loop.runAfter(6000, [&]() { loop.quit(); });
  loop.loop();
  cout << "finished";
}
