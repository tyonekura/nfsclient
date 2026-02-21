cmake_minimum_required(VERSION 3.20)
project(nfsclient CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(tools)
