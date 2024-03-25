#ifndef _LOG_H_
#define _LOG_H_

#include <chrono>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace gdlog {
// 数据生产者部分
class Logger {
 public:
  using OutputFunc = std::function<void(const char* msg, int len)>;
  using FlushFunc = std::function<void()>;
  enum LogLevel {
    // TRACE,
    DEBUG,
    INFO,
    WARN,
    // ERROR,
    FATAL,
    NUM_LOG_LEVELS
  };
  Logger(const char* file, int line, LogLevel level, const char* func = "");
  ~Logger();

  std::ostringstream& stream() { return stream_; }
  static void setLogLevel(LogLevel level);
  static LogLevel logLevel();
  static void setOutput(OutputFunc out);
  static void setFlush(FlushFunc flush);

 private:
  std::ostringstream stream_;
  LogLevel level_;
  int line_;
  std::string basename_;
  std::string func_;

  // static LogLevel g_level_ ;//不初始化就放到.text（只读代码段）里了???
  // static OutputFunc g_output_t_;
  // static FlushFunc g_flush_h_;
  // c++17现代inline语意，编译时从（相同的）重复定义得到唯一定义，
  inline static LogLevel g_level_ = Logger::DEBUG;
  inline static OutputFunc g_output_ = [](const char* msg, int len) {
    std::cout.write(msg, len);
    std::cout << std::endl;
  };
  inline static FlushFunc g_flush_ = []() { std::cout.flush(); };
};
}  // namespace gdlog

// #define LOG_TRACE                                        \
//   if (gdlog::Logger::logLevel() <= gdlog::Logger::TRACE) \
//   gdlog::Logger(__FILE__, __LINE__, gdlog::Logger::TRACE, __func__).stream()
#define LOG_DEBUG                                        \
  if (gdlog::Logger::logLevel() <= gdlog::Logger::DEBUG) \
  gdlog::Logger(__FILE__, __LINE__, gdlog::Logger::DEBUG, __func__).stream()
#define LOG_INFO                                        \
  if (gdlog::Logger::logLevel() <= gdlog::Logger::INFO) \
  gdlog::Logger(__FILE__, __LINE__, gdlog::Logger::INFO).stream()
#define LOG_WARN gdlog::Logger(__FILE__, __LINE__, gdlog::Logger::WARN).stream()
// #define LOG_ERROR \
//   gdlog::Logger(__FILE__, __LINE__, gdlog::Logger::ERROR).stream()
#define LOG_FATAL \
  gdlog::Logger(__FILE__, __LINE__, gdlog::Logger::FATAL).stream()
// #define LOG_SYSERR gdlog::Logger(__FILE__, __LINE__, false).stream()
// #define LOG_SYSFATAL gdlog::Logger(__FILE__, __LINE__, true).stream()

#endif