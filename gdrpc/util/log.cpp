#include "log.hpp"

#include "current_thread.hpp"
#include "time.hpp"
namespace gdrpc {
namespace gdlog {
namespace {
const char* LogLevelName[Logger::NUM_LOG_LEVELS] = {
    //   "TRACE ",
    "DEBUG ",
    "INFO  ",
    "WARN  ",
    //   "ERROR ",
    "FATAL ",
};

}  // namespace

Logger::Logger(const char* file, int line, LogLevel level, const char* func)
    // 不能在这=""感觉反而不直观了,明明应该是和声明处默认值不一样才会重定义啊？？？
    : stream_(), level_(level), line_(line), basename_(file), func_(func) {
  stream_ << util::get_time() << ' ' << CurrentThread::tid() << ' '
          << LogLevelName[level_] << ' ';
}

Logger::~Logger() {
  stream_ << " - " << basename_ << ':';
  if (!func_.empty()) stream_ << func_ << ':';
  stream_ << line_ << '\n';
  g_output_(stream_.str().c_str(), static_cast<int>(stream_.str().length()));
  //   if (impl_.level_ == FATAL) {
  // g_flush();
  // abort();
  //   }
}

Logger::LogLevel Logger::logLevel() { return g_level_; }

void Logger::setLogLevel(Logger::LogLevel level) { g_level_ = level; }

void Logger::setOutput(OutputFunc out) { g_output_ = out; }

void Logger::setFlush(FlushFunc flush) { g_flush_ = flush; }

// Logger::LogLevel Logger::g_level_ = Logger::DEBUG;

// Logger::OutputFunc Logger::g_output_ = [](const char* msg, int len) {
//   std::cout.write(msg, len);
//   std::cout << std::endl;
// };
// Logger::FlushFunc Logger::g_flush_ = []() { std::cout.flush(); };
}  // namespace gdlog
}  // namespace gdrpc