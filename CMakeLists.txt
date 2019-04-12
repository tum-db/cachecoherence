cmake_minimum_required(VERSION 3.10)
project(medmm CXX)

set(CMAKE_CXX_STANDARD 17)
set(WARNINGS "-Wall -Wextra -Wnon-virtual-dtor -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wrestrict")
set(WARNINGS "${WARNINGS} -Wnull-dereference -Wold-style-cast -Wuseless-cast -Wdouble-promotion")
set(WARNINGS "${WARNINGS} -Wformat=2 -Wshadow=local -Wodr")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic ${WARNINGS}")

file(GLOB COMMON_SOURCES
        src/*.cpp
        rdma/*.cpp
        util/*.cpp
        util/socket/*.cpp
        )
file(GLOB COMMON_HEADERS
        src/*.h
        rdma/*.h
        rdma/*.hpp
        util/*.h
        util/socket/*.h
        )

SET(LINK_LIBRARIES
        ibverbs
        )

include_directories(include/libibverbscpp)

add_library(medmm STATIC ${COMMON_SOURCES} ${COMMON_HEADERS})
add_executable(main main.cpp)
target_link_libraries(main medmm)
target_link_libraries(medmm ${LINK_LIBRARIES})