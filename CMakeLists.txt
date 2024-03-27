cmake_minimum_required(VERSION 3.10)  # 或者根据实际情况调整为更高版本

# 定义项目名称
project(MyProject)

# 设置 C++ 标准（例如 C++14）
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)


# 静态库的名字
set(GDRPC_LIB gdrpc)
# 头文件目录
set(HEAD_PATH ${CMAKE_CURRENT_SOURCE_DIR}/gdrpc)
# 静态库生成的路径
set(LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib)
# 添加 gdrpc 子目录
add_subdirectory(gdrpc)

# 将 gdrpc 库设置为全局可见（可选，如果不在同一目录下需要显式指定）
# target_include_directories(gdrpc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/gdrpc/include)

# 处理 example 目录
add_subdirectory(example)

