#include "timestamp.hpp"
namespace gdrpc {
namespace util {

Timestamp::Timestamp(std::chrono::system_clock::time_point tp)
    : time_point_(tp) {}
time_t Timestamp::get_time_t() const {
  return std::chrono::system_clock::to_time_t(time_point_);
}
std::string Timestamp::get_time() const {
  std::time_t tt = get_time_t();
  char timebuf[32];
  struct tm tm;
  gmtime_r(&tt, &tm);  // FIXME: localtime_r ?
  strftime(timebuf, sizeof timebuf, ".%Y%m%d-%H%M%S.", &tm);
  return timebuf;
}
std::string Timestamp::get_time_format_8601() const {
  std::time_t tt = get_time_t();
  std::tm utc_tm = *std::gmtime(&tt);  // UTC time
  std::stringstream ss;
  ss << std::put_time(&utc_tm, "%Y%m%d %H:%M:%S");  // Date and time part
  auto duration = time_point_.time_since_epoch();
  auto subseconds =
      std::chrono::duration_cast<std::chrono::microseconds>(duration).count() %
      1000000;
  ss << '.' << std::setfill('0') << std::setw(6)
     << subseconds;  // Subseconds part
  ss << "Z";         // 'Z' indicates that the time is in UTC

  return ss.str();
}
}  // namespace util
}  // namespace gdrpc