# for each "example/x.cpp", generate target "x"


include_directories(${HEAD_PATH})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/echo.pb.h)
link_directories(${LIB_PATH})
link_libraries(${GDRPC_LIB})

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
file(GLOB_RECURSE all_examples *.cpp)
foreach(v ${all_examples})
    string(REGEX MATCH "example/rpc/.*" relative_path ${v})
    # message(${relative_path})
    string(REGEX REPLACE "example/rpc/" "" target_name ${relative_path})
    string(REGEX REPLACE ".cpp" "" target_name ${target_name})

    add_executable(${target_name} ${v} echo.pb.cc)
    target_link_libraries(${target_name}  zookeeper_mt protobuf)

endforeach()


