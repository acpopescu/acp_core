cmake_minimum_required(VERSION 3.8)

SET(CMAKE_TOOLCHAIN_PREFIX "llvm")
SET(CMAKE_CXX_STANDARD "14")
SET(CMAKE_C_STANDARD "14")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
project (threadtst)

add_executable(main main.cpp)
add_executable(testjq testjq.cpp)