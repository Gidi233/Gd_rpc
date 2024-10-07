#include "rpcServer.hpp"

#include "zkClient.hpp"
// #include "zookeeperutil.h"
#include "../protocol/rpcHeader.pb.h"
#include "../util/log.hpp"

/*
service_name =>  service描述
                        =》 service* 记录服务对象
                        method_name  =>  method方法对象
json   protobuf
*/
namespace gdrpc::rpc {
void RpcServer::RegisterService(google::protobuf::Service* service) {
  const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
  services_[desc->full_name()] = service;
}
void RpcServer::Run() {
  net::InetAddress address(port_, ip_);

  net::TcpServer server(&eventLoop_, address, "RpcServer");

  server.setConnectionCallback(
      [this](const net::TcpConnectionPtr& conn) { onConnection(conn); });

  server.setThreadNum(15);

  // 把当前rpc节点上要发布的服务全部注册到zk上面，让rpc client可以从zk上发现服务
  // session timeout   30s     zkclient 网络I/O线程  1/3 * timeout
  // 时间发送ping消息
  ZkClient zkCli;
  zkCli.Start();
  for (auto& sp : services_) {
    std::string service_path = "/" + sp.first;
    char data[32];
    sprintf(data, "%s %d %d\n", ip_.c_str(), port_, weight_);
    zkCli.Append(service_path.c_str(), data, strlen(data),
                 ZOO_EPHEMERAL);  // 临时节点
  }

  // rpc服务端准备启动，打印信息
  LOG_INFO << "RpcServer start service at ip:" << ip_ << " port:" << port_;

  server.start();
  eventLoop_.loop();
}

void RpcServer::onConnection(const net::TcpConnectionPtr& conn) {
  LOG_INFO << "RpcServer - " << conn->peerAddress().toIpPort() << " -> "
           << conn->localAddress().toIpPort() << " is "
           << (conn->connected() ? "UP" : "DOWN");
  if (conn->connected()) {
    conn->setMessageCallback(
        [this](const net::TcpConnectionPtr& conn, util::Timestamp receiveTime) {
          onMessage(conn, receiveTime);
        });
    // 把channel的生命周期给conn保管
    // conn->setContext(channel);
  }
}
RpcServer::RpcServer(const int port, const std::string ip)
    : ip_(ip),
      port_(port),
      codec_([this](const net::TcpConnectionPtr& conn,
                    const RpcHeaderPtr& headerPtr, util::Timestamp receiveTime,
                    const std::string& payload) {
        onRpcHeader(conn, headerPtr, receiveTime, payload);
      }) {
  LOG_INFO << "RpcServer::ctor - " << this;
}

// RpcServer::RpcServer(const net::TcpConnectionPtr& conn)
//     : codec_([this](const net::TcpConnectionPtr& conn,
//                     const RpcHeaderPtr& headerPtr, util::Timestamp
//                     receiveTime, std::string payload) {
//         onRpcHeader(conn, headerPtr, receiveTime, payload);
//       }),
//       services_(nullptr) {
//   LOG_INFO << "RpcServer::ctor - " << this;
// }

RpcServer::~RpcServer() {
  LOG_INFO << "RpcServer::dtor - " << this;
  // for (const auto& outstanding : outstandings_) {
  //   OutstandingCall out = outstanding.second;
  //   delete out.response;
  //   delete out.done;
  // }
}

void RpcServer::onMessage(const net::TcpConnectionPtr& conn,
                          util::Timestamp receiveTime) {
  codec_.onMessage(conn, receiveTime);
}
void RpcServer::onRpcHeader(const net::TcpConnectionPtr& conn,
                            const RpcHeaderPtr& headerPtr,
                            util::Timestamp receiveTime,
                            const std::string& payload) {
  RpcHeader& message = *headerPtr;

  switch (message.message_type_()) {
    case Request: {
      ErrorCode error = WRONG_PROTO;
      auto it = services_.find(message.service_name_());
      if (it != services_.end()) {
        google::protobuf::Service* service = it->second;
        assert(service != nullptr);
        const google::protobuf::ServiceDescriptor* desc =
            service->GetDescriptor();
        const google::protobuf::MethodDescriptor* method =
            desc->FindMethodByName(message.method_name_());
        if (method) {
          std::unique_ptr<google::protobuf::Message> request(
              service->GetRequestPrototype(method).New());
          if (request->ParseFromString(payload)) {
            google::protobuf::Message* response =
                service->GetResponsePrototype(method).New();
            // response is deleted in doneCallback
            int64_t id = message.msg_id_();
            auto pair = std::make_pair(response, id);
            service->CallMethod(
                method, nullptr, request.get(), response,
                NewCallback(this, &RpcServer::SendRpcResponse, pair, conn));
            // NewCallback(nullptr, [&]() { SendRpcResponse(pair, conn); }));
            error = NO_ERROR;
          } else {
            error = INVALID_REQUEST;
          }
        } else {
          error = NO_METHOD;
        }
      } else {
        error = NO_SERVICE;
      }
      if (error != NO_ERROR) {
        //   RpcHeader response;
        //   response.set_type(RESPONSE);
        //   response.set_id(message.id());
        //   response.set_error(error);
        //   codec_.send(conn_, response);
      }
      break;
    }

    default:
      break;
  }
}

void RpcServer::SendRpcResponse(
    std::pair<google::protobuf::Message*, int64_t> pair,
    const net::TcpConnectionPtr& conn) {
  RpcHeader header;
  header.set_message_type_(Response);
  header.set_msg_id_(pair.second);
  //   header.set_response(response->SerializeAsString());  // FIXME: error
  //   check

  codec_.send(conn, header, pair.first);
}
/*
在框架内部，RpcServer和RpcConsumer协商好之间通信用的protobuf数据类型
service_name method_name args
定义proto的message类型，进行数据头的序列化和反序列化 service_name method_name
args_size 16UserServiceLoginzhang san123456

header_size(4个字节) + header_str + args_str
10 "10"
10000 "1000000"
std::string   insert和copy方法
*/
// 已建立连接用户的读写事件回调
// 如果远程有一个rpc服务的调用请求，那么OnMessage方法就会响应
// void RpcServer::OnMessage(const net::TcpConnectionPtr& conn,
//                           util::Timestamp ts) {
//   std::string recv_buf;
//   conn->tryRead(recv_buf);

//   // 从字符流中读取前4个字节的内容
//   uint32_t header_size = 0;
//   recv_buf.copy((char*)&header_size, 4, 0);

//   //
//   根据header_size读取数据头的原始字符流，反序列化数据，得到rpc请求的详细信息
//   std::string rpc_header_str = recv_buf.substr(4, header_size);
//   ::rpc::RpcHeader rpcHeader;
//   std::string service_name;
//   std::string method_name;
//   uint32_t args_size;
//   if (rpcHeader.ParseFromString(rpc_header_str)) {
//     // 数据头反序列化成功
//     service_name = rpcHeader.service_name();
//     method_name = rpcHeader.method_name();
//     args_size = rpcHeader.args_size();
//   } else {
//     // 数据头反序列化失败
//     std::cout << "rpc_header_str:" << rpc_header_str << " parse error!"
//               << std::endl;
//     return;
//   }

//   // 获取rpc方法参数的字符流数据
//   std::string args_str = recv_buf.substr(4 + header_size, args_size);

//   //   // 打印调试信息
//   //   std::cout << "============================================" <<
//   std::endl;
//   //   std::cout << "header_size: " << header_size << std::endl;
//   //   std::cout << "rpc_header_str: " << rpc_header_str << std::endl;
//   //   std::cout << "service_name: " << service_name << std::endl;
//   //   std::cout << "method_name: " << method_name << std::endl;
//   //   std::cout << "args_str: " << args_str << std::endl;
//   //   std::cout << "============================================" <<
//   std::endl;

//   // 获取service对象和method对象
//   auto it = serviceMap_.find(service_name);
//   if (it == serviceMap_.end()) {
//     std::cout << service_name << " is not exist!" << std::endl;
//     return;
//   }

//   auto mit = it->second.methodMap_.find(method_name);
//   if (mit == it->second.methodMap_.end()) {
//     std::cout << service_name << ":" << method_name << " is not exist!"
//               << std::endl;
//     return;
//   }

//   google::protobuf::Service* service =
//       it->second.service_;  // 获取service对象  new UserService
//   const google::protobuf::MethodDescriptor* method =
//       mit->second;  // 获取method对象  Login

//   // 生成rpc方法调用的请求request和响应response参数
//   google::protobuf::Message* request =
//       service->GetRequestPrototype(method).New();
//   if (!request->ParseFromString(args_str)) {
//     std::cout << "request parse error, content:" << args_str << std::endl;
//     return;
//   }
//   google::protobuf::Message* response =
//       service->GetResponsePrototype(method).New();

//   // 给下面的method方法的调用，绑定一个Closure的回调函数
//   google::protobuf::Closure* done =
//       google::protobuf::NewCallback<RpcServer, const net::TcpConnectionPtr&,
//                                     google::protobuf::Message*>(
//           this, &RpcServer::SendRpcResponse, conn, response);

//   // 在框架上根据远端rpc请求，调用当前rpc节点上发布的方法
//   // new UserService().Login(controller, request, response, done)
//   service->CallMethod(method, nullptr, request, response, done);
// }

// // Closure的回调操作，用于序列化rpc的响应和网络发送
// void RpcServer::SendRpcResponse(const net::TcpConnectionPtr& conn,
//                                 google::protobuf::Message* response) {
//   std::string response_str;
//   if (response->SerializeToString(&response_str))  // response进行序列化
//   {
//     // 序列化成功后，通过网络把rpc方法执行的结果发送会rpc的调用方
//     conn->send(response_str);
//   } else {
//     std::cout << "serialize response_str error!" << std::endl;
//   }
//   conn->shutdown();  // 模拟http的短链接服务，由RpcServer主动断开连接
// }
}  // namespace gdrpc::rpc