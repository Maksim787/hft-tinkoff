cmake_minimum_required(VERSION 3.16)
project(hft_tinkoff)

set(CMAKE_CXX_STANDARD 23)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined -Wall")

include_directories(.)

add_subdirectory(hft_library)
