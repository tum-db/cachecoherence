cmake_minimum_required(VERSION 3.10)
project(masterthesis-efficient-distributed-memory-management)

set(CMAKE_CXX_STANDARD 17)


include_directories(include/libibverbscpp)

add_executable(masterthesis-efficient-distributed-memory-management main.cpp)
target_link_libraries(masterthesis-efficient-distributed-memory-management ibverbs)
