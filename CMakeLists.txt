cmake_minimum_required(VERSION 3.16)
project(mini_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(mini_cpp
    src/main.cpp
)

target_compile_options(mini_cpp PRIVATE -Wall -Wextra -Wpedantic)
