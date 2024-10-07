# for each "example/x.cpp", generate target "x"


include_directories(${HEAD_PATH})
link_directories(${LIB_PATH})
link_libraries(${GDRPC_LIB})
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
file(GLOB all_examples *.cpp)
foreach(v ${all_examples})
    string(REGEX MATCH "example/.*" relative_path ${v})
    # message(${relative_path})
    string(REGEX REPLACE "example/" "" target_name ${relative_path})
    string(REGEX REPLACE ".cpp" "" target_name ${target_name})

    add_executable(${target_name} ${v})
endforeach()


add_subdirectory(rpc)