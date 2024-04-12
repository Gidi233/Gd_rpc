#ifndef _LOG_H_
#define _LOG_H_

#include <cstring>
#include <functional>
#include <iostream>
#include <sstream>
namespace gdrpc {
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
inline std::string err_msg(int err = errno) {
  char err_msg[BUFSIZ];
  strerror_r(err, err_msg, sizeof(err_msg));  // 不知为啥 读不出msg
  std::stringstream s;
  s << err << " msg:";
  s << err_msg;
  return s.str();
}
}  // namespace gdlog
}  // namespace gdrpc
// #define LOG_TRACE                                        \
//   if (gdrpc::gdlog::Logger::logLevel() <= gdrpc::gdlog::Logger::TRACE) \
//   gdrpc::gdlog::Logger(__FILE__, __LINE__, gdrpc::gdlog::Logger::TRACE, __func__).stream()
#define LOG_DEBUG                                                       \
  if (gdrpc::gdlog::Logger::logLevel() <= gdrpc::gdlog::Logger::DEBUG)  \
  gdrpc::gdlog::Logger(__FILE__, __LINE__, gdrpc::gdlog::Logger::DEBUG, \
                       __func__)                                        \
      .stream()
#define LOG_INFO                                                      \
  if (gdrpc::gdlog::Logger::logLevel() <= gdrpc::gdlog::Logger::INFO) \
  gdrpc::gdlog::Logger(__FILE__, __LINE__, gdrpc::gdlog::Logger::INFO).stream()
#define LOG_WARN \
  gdrpc::gdlog::Logger(__FILE__, __LINE__, gdrpc::gdlog::Logger::WARN).stream()
// #define LOG_ERROR \
//   gdrpc::gdlog::Logger(__FILE__, __LINE__, gdrpc::gdlog::Logger::ERROR).stream()
#define LOG_FATAL \
  gdrpc::gdlog::Logger(__FILE__, __LINE__, gdrpc::gdlog::Logger::FATAL).stream()
// #define LOG_SYSERR gdrpc::gdlog::Logger(__FILE__, __LINE__, false).stream()
// #define LOG_SYSFATAL gdrpc::gdlog::Logger(__FILE__, __LINE__, true).stream()

#define ERR_MSG(err) gdrpc::gdlog::err_msg(err)

#endif