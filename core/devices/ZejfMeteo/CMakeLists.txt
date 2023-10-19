cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)

set(SOME_FLAGS "-O2 -g -std=gnu11 -Wall -Wextra -Wpedantic")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SOME_FLAGS}")
set(CMAKE_C_CLANG_TIDY "clang-tidy")
set(CMAKE_C_CPPCHECK "cppcheck")

project(zejfmeteo)

set(EXECUTABLE zejfmeteo_beta_1.5.0)

add_subdirectory(src)

add_subdirectory(../../api/zejf_api zejf_api)

target_include_directories(zejf_api PUBLIC src)

target_link_libraries(zejfmeteo zejf_api)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${EXECUTABLE})


