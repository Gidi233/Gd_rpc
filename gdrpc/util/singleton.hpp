
#ifndef _SINGLETON_H_
#define _SINGLETON_H_

#include <assert.h>
#include <stdlib.h>  // atexit

#include <mutex>
namespace gdrpc {
namespace util {
// 利用SFINAE规则，推T中是否有no_destroy成员函数
// This doesn't detect inherited member functions!
// http://stackoverflow.com/questions/1966362/sfinae-to-check-for-inherited-member-functions
template <typename T>
struct has_no_destroy {
  template <typename C>
  static char test(decltype(&C::no_destroy));
  template <typename C>
  static int32_t test(...);
  const static bool value = sizeof(test<T>(0)) == 1;
};

template <typename T>
class Singleton {
 public:
  Singleton() = delete;
  ~Singleton() = delete;

  static T& instance() {
    std::call_once(init_flag_, &Singleton::init);
    assert(value_ != nullptr);
    return *value_;
  }

 private:
  static void init() {
    value_ = new T();
    if constexpr (!has_no_destroy<T>::value) {
      ::atexit(destroy);
    }
  }

  static void destroy() {
    typedef char
        T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];  // 确定有T这个类型
    T_must_be_complete_type dummy;
    (void)dummy;  // 避免未使用warning

    delete value_;
    value_ = nullptr;
  }

  inline static std::once_flag init_flag_ = {};
  static T* value_ = nullptr;
};
}  // namespace util
}  // namespace gdrpc

#endif