#include "zkClient.hpp"

#include <semaphore.h>

#include <iostream>

#include "../util/log.hpp"

namespace gdrpc::rpc {  // 全局的watcher观察器   zkserver给zkclient的通知
void global_watcher(zhandle_t* zh, int type, int state, const char* path,
                    void* watcherCtx) {
  if (type == ZOO_SESSION_EVENT)  // 回调的消息类型是和会话相关的消息类型
  {
    if (state == ZOO_CONNECTED_STATE)  // zkclient和zkserver连接成功
    {
      sem_t* sem = (sem_t*)zoo_get_context(zh);
      sem_post(sem);
    }
  }
}

ZkClient::ZkClient() : zkHandle_(nullptr) {}

ZkClient::~ZkClient() {
  if (zkHandle_ != nullptr) {
    zookeeper_close(zkHandle_);  // 关闭句柄，释放资源  MySQL_Conn
  }
}

// 连接zkserver
void ZkClient::Start() {
  std::string connstr = "localhost:2181";

  /*
  zookeeper_mt：多线程版本
  zookeeper的API客户端程序提供了三个线程
  API调用线程
  网络I/O线程  pthread_create  poll
  watcher回调线程 pthread_create
  */
  zkHandle_ = zookeeper_init(connstr.c_str(), global_watcher, 30000, nullptr,
                             nullptr, 0);
  if (nullptr == zkHandle_) {
    std::cout << "zookeeper_init error!";
    exit(EXIT_FAILURE);
  }

  sem_t sem;
  sem_init(&sem, 0, 0);
  zoo_set_context(zkHandle_, &sem);

  sem_wait(&sem);  // 阻塞到这里，直到信号量不为0
  LOG_INFO << "zookeeper_init success!";
}

int ZkClient::Append(const char* path, const char* data, int datalen,
                     int state) {
  char path_buffer[128];
  int bufferlen = sizeof(path_buffer);
  int flag;
  // 先判断path表示的znode节点是否存在，如果存在，就不再重复创建了
  flag = zoo_exists(zkHandle_, path, 0, nullptr);
  if (ZNONODE == flag)  // 表示path的znode节点不存在
  {
    // 创建指定path的znode节点了
    flag = zoo_create(zkHandle_, path, data, datalen, &ZOO_OPEN_ACL_UNSAFE,
                      state, path_buffer, bufferlen);
    if (flag == ZOK) {
      LOG_INFO << "znode Append success... path:" << path;
    } else {
      LOG_WARN << "flag:" << flag;
      LOG_WARN << "znode Append error... path:" << path;
    }
  }
  if (flag == ZOK) {
    char buf[1024];
    int len = sizeof(buf);
    flag = zoo_get(zkHandle_, path, 0, buf, &len, nullptr);
    if (flag != ZOK) {
      LOG_WARN << "flag:" << flag;
      LOG_WARN << "znode get error... path:" << path;
    }
    std::string s(buf, len);
    s.append(data, datalen);
    flag = zoo_set(zkHandle_, path, s.c_str(), s.length(),
                   -1);  // 最后这个参数检查期间是否有其他的修改
    if (flag == ZOK) {
      LOG_INFO << "znode append success... path:" << path;
    } else {
      LOG_WARN << "flag:" << flag;
      LOG_WARN << "znode set error... path:" << path;
    }
  }
  return flag;
}

// 根据指定的path，获取znode节点的值
std::string ZkClient::GetData(const char* path) {
  char buffer[1024];
  int bufferlen = sizeof(buffer);
  int flag = zoo_get(zkHandle_, path, 0, buffer, &bufferlen, nullptr);
  if (flag != ZOK) {
    std::cout << "get znode error... path:" << path;
    return "";
  } else {
    return buffer;
  }
}
}  // namespace gdrpc::rpc