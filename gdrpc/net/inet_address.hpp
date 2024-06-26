#ifndef _INET_ADDRESS_
#define _INET_ADDRESS_
#include <arpa/inet.h>
#include <netinet/in.h>

#include <string>

namespace gdrpc {
namespace net {
// 辅助ip地址类
class InetAddress {
 public:
  explicit InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1");
  explicit InetAddress(const sockaddr_in& addr) : addr_(addr) {}

  std::string toIp() const;
  std::string toIpPort() const;
  uint16_t toPort() const;
  sa_family_t family() const { return addr_.sin_family; }

  const sockaddr_in* getSockAddr() const { return &addr_; }
  void setSockAddr(const sockaddr_in& addr) { addr_ = addr; }

 private:
  sockaddr_in addr_;
};
}  // namespace net
}  // namespace gdrpc
#endif