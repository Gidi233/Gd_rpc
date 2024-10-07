#include "tcpServer.hpp"

#include <string.h>

#include <functional>

#include "../util/log.hpp"
namespace gdrpc {
namespace net {
static EventLoop* CheckLoopNotNull(EventLoop* loop) {
  if (loop == nullptr) {
    LOG_FATAL << " mainLoop is null!\n";
  }
  return loop;
}

TcpServer::TcpServer(EventLoop* loop, const InetAddress& listenAddr,
                     const std::string& nameArg, Option option)
    : loop_(CheckLoopNotNull(loop)),
      addr_(listenAddr),
      name_(nameArg),
      acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),
      threadPool_(new EventLoopThreadPool(loop, name_)),
      nextConnId_(1),
      started_(0) {
  // 当有新用户连接时，Acceptor类中绑定的acceptChannel_会有读事件发生，执行handleRead()调用TcpServer::newConnection回调
  acceptor_->setNewConnectionCallback(
      [this](int sockfd, const InetAddress& peerAddr) {
        newConnection(sockfd, peerAddr);
      });
}

TcpServer::~TcpServer() {
  LOG_DEBUG << "TcpServer::~TcpServer [" << name_ << "] destructing";
  for (auto& item : connections_) {
    item.second->getLoop()->runInLoop(
        std::bind(&TcpConnection::connectDestroyed, item.second));
  }
}

// 设置底层subloop的个数
void TcpServer::setThreadNum(int numThreads) {
  threadPool_->setThreadNum(numThreads);
}

void TcpServer::start() {
  if (started_++ == 0)  // 防止一个TcpServer对象被start多次
  {
    threadPool_->start(threadInitCallback_);
    loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
  }
}

// 有一个新用户连接，acceptor会执行这个回调操作，将接收到的请求连接分发给subLoop处理
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr) {
  EventLoop* ioLoop = threadPool_->getNextLoop();
  char buf[64] = {0};
  snprintf(buf, sizeof buf, "-%s#%d", addr_.toIpPort().c_str(), nextConnId_);
  ++nextConnId_;
  std::string connName = name_ + buf;

  LOG_INFO << "TcpServer::newConnection [" << name_ << "] - new connection ["
           << connName << "] from " << peerAddr.toIpPort();

  TcpConnectionPtr conn = std::make_shared<TcpConnection>(
      ioLoop, connName, sockfd, addr_, peerAddr);
  connections_[connName] = conn;
  // 下面的回调都是用户设置给TcpServer =>
  // TcpConnection的，后传递给Channel的handlexxx中
  conn->setConnectionCallback(connectionCallback_);
  conn->setMessageCallback(messageCallback_);
  conn->setWriteCompleteCallback(writeCompleteCallback_);

  // 设置了如何关闭连接的回调
  conn->setCloseCallback([this](const TcpConnectionPtr& conn) {
    // 删除TcpServer维护的map交由主循环处理
    loop_->runInLoop([&]() { removeConnectionInLoop(conn); });
  });

  ioLoop->runInLoop([conn]() { conn->connectEstablished(); });
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn) {
  LOG_INFO << "TcpServer::removeConnectionInLoop [" << name_
           << "] - connection " << conn->name();

  connections_.erase(conn->name());
  EventLoop* ioLoop = conn->getLoop();
  // 加到待办队列是为了保留tcpconnect的生命周期，直到已添加进loop的事件执行完（且handleclose改完state后不会有新事件增加），再destroy。
  // 而且当前函数执行在主循环，也应把Destroy交给对应子循环执行
  ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}
}  // namespace net
}  // namespace gdrpc