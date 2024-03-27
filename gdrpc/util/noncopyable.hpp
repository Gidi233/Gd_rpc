#ifndef _NONCOPYABLE_
#define _NONCOPYABLE_

/**
 * noncopyable被继承后 派生类对象可正常构造和析构
 *但派生类对象无法进行拷贝构造和赋值构造
 **/
class noncopyable {
 public:
  noncopyable(const noncopyable&) = delete;
  noncopyable& operator=(const noncopyable&) = delete;

 protected:
  noncopyable() = default;
  ~noncopyable() = default;
};
#endif
