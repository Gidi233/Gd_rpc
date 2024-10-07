#include "tcpClient.hpp"

#include <stdio.h>

#include "../util/log.hpp"
#include "eventLoop.hpp"

namespace gdrpc::net {

TcpClient::TcpClient(EventLoop* loop, const InetAddress& serverAddr,
                     const std::string& nameArg)
    : loop_(loop),
      connector_(new Connector(loop, serverAddr)),
      name_(nameArg),
      retry_(false),
      connect_(false),
      nextConnId_(1),
      latch_(false) {
  connector_->setNewConnectionCallback(
      [this](std::unique_ptr<Channel>&& channel_) {
        newConnection(std::move(channel_));
      });
  LOG_INFO << "TcpClient::TcpClient[" << name_ << "] - connector "
           << connector_.get();
}

TcpClient::~TcpClient() {
  LOG_INFO << "TcpClient::~TcpClient[" << name_ << "] - connector "
           << connector_.get();

  TcpConnectionPtr conn;
  {
    std::unique_lock lock(mutex_);
    conn = connection_;
  }
  if (conn) {
    CloseCallback cb = [&](const TcpConnectionPtr& conn_) {
      // removeConnection dirctly
      loop_->queueInLoop([conn_]() { conn_->connectDestroyed(); });
    };
    loop_->runInLoop(
        // 按理说lambda捕获的值都为const，调用设置回调再传入&&没有效果，但加了mutable
        // 有以下问题：warning: parameter declaration
        // before lambda declaration specifiers only
        // optional with ‘-std=c++2b’ or ‘-std=gnu++2b’
        [this, cb_ = std::move(cb)] /*mutable*/ {
          connection_->setCloseCallback(cb_);
        });
    loop_->queueInLoop([conn] { conn->forceClose(); });
  } else {
    connector_->stop();
  }
}

void TcpClient::connect() {
  if (!connect_) {
    LOG_INFO << "TcpClient::connect[" << name_ << "] - connecting to "
             << connector_->serverAddress().toIpPort();
    connect_ = true;
    connector_->start();
  }
}

void TcpClient::already() {
  latch_.acquire();
  LOG_INFO << "TcpClient::already";
}

void TcpClient::disconnect() {
  connect_ = false;
  {
    std::unique_lock lock(mutex_);
    if (connection_) {
      connection_->shutdown();
    }
  }
}

void TcpClient::stop() {
  connect_ = false;
  connector_->stop();
}

// 这一整个传递Channel调用链，我认为用&&能避免传参时反复构造智能指针
// 正常使用&&是用来匹配另一种消耗更少（把参数拿来就用，并替调用者管理参数的生命周期）的同功能函数的
void TcpClient::newConnection(std::unique_ptr<Channel>&& channel) {
  int sockfd = channel->fd();
  struct sockaddr_in addr_;
  memset(&addr_, 0, sizeof addr_);
  socklen_t addrlen = static_cast<socklen_t>(sizeof addr_);
  if (::getpeername(sockfd, reinterpret_cast<struct sockaddr*>(&addr_),
                    &addrlen) < 0) {
    LOG_FATAL << "sockets::getPeerAddr";
  }

  InetAddress peerAddr(peerAddr);
  char buf[32];
  snprintf(buf, sizeof buf, ":%s#%d", peerAddr.toIpPort().c_str(), nextConnId_);
  ++nextConnId_;
  std::string connName = name_ + buf;

  memset(&addr_, 0, sizeof addr_);
  if (::getsockname(sockfd, reinterpret_cast<struct sockaddr*>(&addr_),
                    &addrlen) < 0) {
    LOG_FATAL << "sockets::getLocalAddr";
  }
  InetAddress localAddr(addr_);
  TcpConnectionPtr conn = std::make_shared<TcpConnection>(
      loop_, connName, std::move(channel), localAddr, peerAddr);

  conn->setConnectionCallback(connectionCallback_);
  conn->setMessageCallback(messageCallback_);
  conn->setWriteCompleteCallback(writeCompleteCallback_);
  conn->setCloseCallback(
      [this](const TcpConnectionPtr& conn) { removeConnection(conn); });
  {
    std::unique_lock lock(mutex_);
    connection_ = conn;
  }
  conn->connectEstablished();
  latch_.release();
}

void TcpClient::removeConnection(const TcpConnectionPtr& conn) {
  {
    std::unique_lock lock(mutex_);
    connection_.reset();
  }

  loop_->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
  if (retry_ && connect_) {
    LOG_INFO << "TcpClient::connect[" << name_ << "] - Reconnecting to "
             << connector_->serverAddress().toIpPort();
    connector_->restart();
  }
}
// void TcpClient::writeMessage(
//     AbstractProtocol::s_ptr message,
//     std::function<void(AbstractProtocol::s_ptr)> done) {
//   // 1. 把 message 对象写入到 Connection 的 buffer, done 也写入
//   // 2. 启动 connection 可写事件
//   connection_->connection_->pushSendMessage(message, done);
//   connection_->listenWrite();
// }

// // 异步的读取 message
// // 如果读取 message 成功，会调用 done 函数， 函数的入参就是 message 对象
// void TcpClient::readMessage(const std::string& msg_id,
//                             std::function<void(AbstractProtocol::s_ptr)>
//                             done) {
//   // 1. 监听可读事件
//   // 2. 从 buffer 里 decode 得到 message 对象, 判断是否 msg_id
//   // 相等，相等则读成功，执行其回调
//   connection_->pushReadMessage(msg_id, done);
//   connection_->listenRead();
// }

}  // namespace gdrpc::net
