cmake_minimum_required(VERSION 3.10)
project(medmm CXX)

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

add_library(medmm STATIC ${COMMON_SOURCES} ${COMMON_HEADERS})
add_executable(main main.cpp)
target_link_libraries(main medmm)
target_link_libraries(medmm ${LINK_LIBRARIES})