#ifndef _ASYNC_LOG_
#define _ASYNC_LOG_
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <semaphore>
#include <string>
#include <thread>
#include <vector>

#include "noncopyable.hpp"
#include "timestamp.hpp"

namespace gdrpc {
namespace gdlog {
class AsyncLogging : noncopyable {
 public:
  AsyncLogging(const std::string& basename, off_t rollSize,
               int flushInterval = 3);

  ~AsyncLogging() {
    if (running_) {
      stop();
    }
  }

  void append(const char* logline, int len);
  void flush() { file_.flush(); }
  void stop() {
    running_ = false;
    cond_.notify_one();
    thread_.join();
  }

 private:
  class Buffer {
   public:
    static constexpr int size_ = 4000 * 1024;
    char* data_;
    char* cur_;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer() { cur_ = data_ = (char*)malloc(size_); }
    ~Buffer() { free(data_); }

    const char* data() const { return data_; }
    int length() const { return static_cast<int>(cur_ - data_); }
    void reset() { cur_ = data_; }
    void bzero() { memset(data_, 0, size_); }
    int avail() const { return static_cast<int>(end() - cur_); }
    void append(const char* buf, int len) {
      if (avail() > len) {
        memcpy(cur_, buf, len);
        cur_ += len;
      }
    }

   private:
    const char* end() const { return data_ + size_; }
  };
  class LogFile {
   public:
    LogFile(const std::string& basename, off_t rollSize, int flushInterval = 3,
            int checkEveryN = 1024);
    ~LogFile() = default;

    void append(const char* logline, int len);
    void flush() { file_.flush(); }

   private:
    bool rollFile();

    std::string getLogFileName(const std::string& basename,
                               const util::Timestamp& ts);

    const std::string basename_;
    const off_t rollSize_;
    const int flushInterval_;
    const int checkEveryN_;

    int count_;

    std::mutex mutex_;
    time_t startOfPeriod_;
    time_t lastRoll_;
    time_t lastFlush_;
    std::ofstream file_;
    int cur_len_;

    const static int kRollPerSeconds_ = 60 * 60 * 24;
  };

  void threadFunc();

  using BufferVector = std::vector<std::unique_ptr<Buffer>>;
  using BufferPtr = BufferVector::value_type;

  const int flushInterval_;
  std::atomic<bool> running_;
  std::thread thread_;
  std::binary_semaphore latch_;
  std::mutex mutex_;
  std::condition_variable cond_;
  BufferPtr currentBuffer_;
  BufferPtr nextBuffer_;
  BufferVector buffers_;
  LogFile file_;
};

}  // namespace gdlog
}  // namespace gdrpc
#endif