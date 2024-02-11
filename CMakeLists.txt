cmake_minimum_required(VERSION 3.26)
project(csc_third)

set(CMAKE_CXX_STANDARD 17)

add_executable(server server.cpp)
target_link_libraries(server PRIVATE ws2_32)

add_executable(client client.cpp)
target_link_libraries(client PRIVATE ws2_32)
