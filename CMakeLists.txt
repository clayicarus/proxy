cmake_minimum_required(VERSION 3.23)
project(proxy)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_EXE_LINKER_FLAGS -static)

add_executable(tcp_relay
        tcprelay.cpp)
target_link_libraries(tcp_relay muduo_net muduo_base)

add_executable(socks
        socks4a.cpp Socks.cpp Socks.h)
target_link_libraries(socks muduo_net muduo_base)
