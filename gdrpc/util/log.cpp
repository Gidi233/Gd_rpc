#include "log.hpp"
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

std::string format_time_iso_8601(
    const std::chrono::system_clock::time_point& tp) {
  std::time_t tt = std::chrono::system_clock::to_time_t(tp);
  std::tm utc_tm = *std::gmtime(&tt);  // UTC time

  std::stringstream ss;
  ss << std::put_time(&utc_tm, "%Y%m%d %H:%M:%S");  // Date and time part
  auto duration = tp.time_since_epoch();
  auto subseconds =
      std::chrono::duration_cast<std::chrono::microseconds>(duration).count() %
      1000000;
  ss << '.' << std::setfill('0') << std::setw(6)
     << subseconds;  // Subseconds part
  ss << "Z";         // 'Z' indicates that the time is in UTC

  return ss.str();
}

}  // namespace

Logger::Logger(const char* file, int line, LogLevel level, const char* func)
    // 不能在这=""感觉反而不直观了,明明应该是和声明处默认值不一样才会重定义啊？？？
    : stream_(), level_(level), line_(line), basename_(file), func_(func) {
  stream_ << format_time_iso_8601(std::chrono::system_clock::now())
          << ' '
          // <<threadid_ << ' '
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