cmake_minimum_required(VERSION 3.16)

project(decompressor)

find_library(LZ4 lz4)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(decompression)

add_executable(decompressor main.cpp)

target_link_libraries(decompressor PUBLIC decompression "${LZ4}")