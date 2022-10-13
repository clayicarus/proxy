cmake_minimum_required(VERSION 3.23)
project(proxy)

set(CMAKE_CXX_STANDARD 14)

add_executable(tcp_relay
        tcprelay.cpp)
target_link_libraries(tcp_relay muduo_net muduo_base)
