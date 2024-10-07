#ifndef _ZKCLIENT_
#define _ZKCLIENT_
#include <semaphore.h>
#include <zookeeper/zookeeper.h>

#include <string>
namespace gdrpc::rpc {
// 封装的zk客户端类
class ZkClient {
 public:
  ZkClient();
  ~ZkClient();
  // zkclient启动连接zkserver
  void Start();
  // 在zkserver上根据指定的path创建znode节点
  int Append(const char* path, const char* data, int datalen, int state = 0);
  // 根据参数指定的znode节点路径，或者znode节点的值
  std::string GetData(const char* path);

 private:
  // zk的客户端句柄
  zhandle_t* zkHandle_;
};
using ZkClientPtr = std::shared_ptr<ZkClient>;
}  // namespace gdrpc::rpc
#endif