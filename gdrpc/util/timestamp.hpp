#ifndef _TIME_
#define _TIME_
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>
namespace gdrpc {
namespace util {
class Timestamp {
 public:
  Timestamp() : time_point_(std::chrono::system_clock::now()) {}
  explicit Timestamp(std::chrono::system_clock::time_point tp);
  Timestamp(const Timestamp&) = default;
  Timestamp& operator=(const Timestamp&) = default;
  time_t get_time_t() const;
  int64_t get_ms() const;
  // static void set_time_zone(const std::string& tz);
  // gmtime() UTC time  or localtime_r
  std::string get_time() const;
  std::string get_time_format_8601() const;

 private:
  std::chrono::_V2::system_clock::time_point time_point_;
};
}  // namespace util
}  // namespace gdrpc
#endif