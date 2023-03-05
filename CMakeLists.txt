cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_COMPILER "g++")

set(ALL_FLAGS "-O2 -g -std=gnu11 -Wall -Wextra -Wpedantic")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ALL_FLAGS}")
set(CMAKE_C_CLANG_TIDY "clang-tidy;--config-file=/home/jakublinux/Desktop/All/CProjects/ZejfMeteo/.clang-tidy")
set(CMAKE_C_CPPCHECK "cppcheck")

set(EXECUTABLE zejfmeteo_beta_1.1.2)

# Project configuration
project(zejfmeteo)

include_directories(lib/zejf_api/inc)

add_subdirectory(src)
add_subdirectory(lib)

target_include_directories(zejf_api PUBLIC src)

target_link_libraries(zejfmeteo zejf_api)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${EXECUTABLE})


