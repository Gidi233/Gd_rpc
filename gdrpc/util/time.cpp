#include "time.hpp"
namespace gdrpc {
namespace util {
std::string get_time() {
  auto tp = std::chrono::system_clock::now();
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
}  // namespace util
}  // namespace gdrpc