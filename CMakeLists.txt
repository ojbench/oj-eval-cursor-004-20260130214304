cmake_minimum_required(VERSION 3.10)
project(bookstore CXX)

set(CMAKE_CXX_STANDARD 17)
add_executable(code main.cpp)
target_compile_options(code PRIVATE -O2 -Wall)
