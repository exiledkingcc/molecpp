cmake_minimum_required(VERSION 3.5)
project(molecpp)

set(CMAKE_CXX_STANDARD 17)
add_compile_options(-Wall -Wextra -fPIC)

include_directories(thirdparty/include)

add_subdirectory(src)
