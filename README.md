# Gd_rpc

采用主从 Reactor 架构和 one-loop-per-thread 模型，通过非阻塞 IO , Epoll , ThreadPool 提高并发性能，使用异步回调机制对事件进行分发和处理。实现了支持按日期和大小的日志滚动、支持多种日志级别的多缓冲区异步日志。基于 Protobuf 构建自定义协议。通过时间轮实现高效的心跳、超时与重试机制。利用 ZooKeeper 进行服务注册和服务发现，在客户端维护多个服务后端的连接并实现了平滑的加权负载均衡。

## 安装zookeeper c api client 
[遇到问题 有用的信息](https://stackoverflow.com/questions/64916994/build-xml-does-not-exist-when-install-zookeeper-client-c) 
```bash
sudo apt install libcppunit-dev
cd zookeeper-jute
mvn compile
cd ../zookeeper-client/zookeeper-client-c
autoconf -if
./configure
make
make install
```


## 构建
```bash
protoc gdrpc/protocol/rpcHeader.proto --cpp_out=.
protoc example/rpc/echo.proto --cpp_out=.
mkdir build
cd build
cmake ..
make all 或 cmake --build . --target all  
// 清理
make clean 或 cmake --build . --target clean  
```


## 相关笔记
[基础网络库](https://blog.csdn.net/qq_74050480/article/details/137730499)
[rpc设计](https://blog.csdn.net/qq_74050480/article/details/142747708)
[关于回调与协程](https://blog.csdn.net/qq_74050480/article/details/136888546)

[基于编译时反射的通用序列化引擎](https://github.com/qicosmos/iguana)(本来想抽象可以使用不同的序列化协议，但得继承 PB 的message 不好实现，遂放弃)
