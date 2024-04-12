#ifndef _TCPCONNECTION_
#define _TCPCONNECTION_
#include <atomic>
#include <memory>
#include <string>

#include "../util/noncopyable.hpp"
#include "../util/timestamp.hpp"
#include "callback.hpp"
#include "inet_address.hpp"
namespace gdrpc {
namespace net {
class Channel;
class EventLoop;
class Socket;

// enable_shared_from_this延长生命周期
class TcpConnection : noncopyable,
                      public std::enable_shared_from_this<TcpConnection> {
 public:
  TcpConnection(EventLoop* loop, const std::string& nameArg, int sockfd,
                const InetAddress& localAddr, const InetAddress& peerAddr);
  TcpConnection(EventLoop* loop, const std::string& nameArg,
                std::unique_ptr<Channel>&& channel,
                const InetAddress& localAddr, const InetAddress& peerAddr);
  ~TcpConnection();

  EventLoop* getLoop() const { return loop_; }
  const std::string& name() const { return name_; }
  const InetAddress& localAddress() const { return localAddr_; }
  const InetAddress& peerAddress() const { return peerAddr_; }

  bool connected() const { return state_ == kConnected; }

  size_t tryRead(std::string& s) {
    size_t n = inputBuffer_.readableBytes();
    s = std::string(inputBuffer_.peek(), n);
    return n;
  };
  bool retrieve(size_t n) { return inputBuffer_.retrieve(n); };
  size_t readAll(std::string& s) {
    int n = tryRead(s);
    retrieve(n);
    return n;
  }

  void send(const std::string& buf);
  void shutdown();
  // void startRead();
  // void stopRead();
  // void startReadInLoop();
  // void stopReadInLoop();

  void setConnectionCallback(const ConnectionCallback& cb) {
    connectionCallback_ = cb;
  }
  void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
  void setWriteCompleteCallback(const WriteCompleteCallback& cb) {
    writeCompleteCallback_ = cb;
  }
  void setCloseCallback(const CloseCallback& cb) { closeCallback_ = cb; }
  void setHighWaterMarkCallback(const HighWaterMarkCallback& cb,
                                size_t highWaterMark) {
    highWaterMarkCallback_ = cb;
    highWaterMark_ = highWaterMark;
  }

  void connectEstablished();
  void connectDestroyed();
  void forceClose();

 private:
  //  shutdown后输出buffer和eventloop中还有未传输的数据，所以需要kDisconnecting，执行完shutdown的回调后，等待接收到客户端close事件后彻底关闭
  enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };
  const char* get_state() const;
  void setState(StateE state) { state_ = state; }

  void handleRead(util::Timestamp receiveTime);
  void handleWrite();
  void handleClose();
  void handleError();

  void forceCloseInLoop();
  void sendInLoop(const void* data, size_t len);
  void shutdownInLoop();

  // 若为多Reactor 该loop_指向subloop 若为单Reactor 该loop_指向baseloop
  EventLoop* loop_;
  const std::string name_;
  // 可能别的连接（另一个线程）在交互时发现该连接（或拥有该连接的上层结构）状态异常，通过shutdown改变状态，需要atomic
  // 感觉其他线程能更改该状态的话就破坏了one loop per thread,
  // 只不过shutdown只是注册回调，不会有两个线程同时修改该资源
  std::atomic<StateE> state_;
  bool reading_;

  // Channel 这里和Acceptor类似
  // Acceptor => mainloop TcpConnection => subloop
  std::unique_ptr<Channel> channel_;

  const InetAddress localAddr_;
  const InetAddress peerAddr_;

  // TcpServer再将注册的回调传递给TcpConnection
  // TcpConnection再将回调注册到Channel中
  ConnectionCallback connectionCallback_;
  MessageCallback messageCallback_;
  WriteCompleteCallback writeCompleteCallback_;
  HighWaterMarkCallback highWaterMarkCallback_;
  CloseCallback closeCallback_;
  size_t highWaterMark_;

  class Buffer {
   public:
    static constexpr size_t kInitialSize = 1024;

    explicit Buffer(size_t initalSize = kInitialSize)
        : buffer_(initalSize), readerIndex_(0), writerIndex_(0) {}

    size_t readableBytes() const { return writerIndex_ - readerIndex_; }
    size_t writableBytes() const { return buffer_.size() - writerIndex_; }
    size_t prependableBytes() const { return readerIndex_; }

    const char* peek() const { return begin() + readerIndex_; }
    bool retrieve(size_t len);
    // 刷新缓冲区
    void retrieveAll() {
      readerIndex_ = 0;
      writerIndex_ = 0;
    }

    std::string retrieveAllAsString() {
      return retrieveAsString(readableBytes());
    }
    std::string retrieveAsString(size_t len) {
      std::string result(peek(), len);
      retrieve(len);
      return result;
    }

    void ensureWritableBytes(size_t len) {
      if (writableBytes() < len) {
        makeSpace(len);
      }
    }

    void append(const char* data, size_t len) {
      ensureWritableBytes(len);
      std::copy(data, data + len, beginWrite());
      writerIndex_ += len;
    }
    char* beginWrite() { return begin() + writerIndex_; }
    const char* beginWrite() const { return begin() + writerIndex_; }

    ssize_t readFd(int fd, int* saveErrno);
    ssize_t writeFd(int fd, int* saveErrno);

   private:
    // vector底层数组首元素的地址
    char* begin() { return &*buffer_.begin(); }
    const char* begin() const { return &*buffer_.begin(); }

    void makeSpace(size_t len);

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
  };

  Buffer inputBuffer_;
  Buffer outputBuffer_;
};
}  // namespace net
}  // namespace gdrpc
#endif