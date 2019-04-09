cmake_minimum_required(VERSION 3.10)
project(masterthesis-efficient-distributed-memory-managemen)

set(CMAKE_CXX_STANDARD 17)

file(GLOB COMMON_SOURCES
        src/*.cpp
        )
file(GLOB COMMON_HEADERS
        src/*.h
        )

SET(LINK_LIBRARIES
        ibverbs
        )

include_directories(include/libibverbscpp)
add_library(masterthesis-efficient-distributed-memory-management STATIC ${COMMON_SOURCES} ${COMMON_HEADERS})

add_executable(masterthesis-efficient-distributed-memory-management main.cpp)
target_link_libraries(masterthesis-efficient-distributed-memory-management ${LINK_LIBRARIES})
