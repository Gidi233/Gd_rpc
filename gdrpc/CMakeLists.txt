cmake_minimum_required(VERSION 3.0)
project(gdrpc)
set(CMAKE_CXX_STANDARD 20)
file(GLOB_RECURSE SRC_LIST "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
include_directories(HEAD_PATH)

# 设置动态库/静态库生成路径
set(LIBRARY_OUTPUT_PATH ${LIB_PATH})
# 生成静态库
add_library(gdrpc STATIC ${SRC_LIST})
# include_directories(${PROTOBUF_INCLUDE_DIRS})
# include_directories(${ZOOKEEPER_INCLUDE_DIRS})
# find_package(zookeeper_mt)
# find_package(protobuf)

target_link_libraries(gdrpc zookeeper_mt protobuf)

# 包含库路径
# link_directories(${PROJECT_SOURCE_DIR}/lib)
# 使用 target_link_libraries 命令就可以链接动态库，也可以链接静态库文件。
# 链接静态库
# link_libraries(calc)