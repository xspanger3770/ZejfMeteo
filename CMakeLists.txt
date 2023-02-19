cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)


set(ALL_FLAGS "-O2 -g -std=gnu99 -Wall -Wextra -Wpedantic")
#set(ALL_FLAGS "-O2 -s -std=gnu99 -Wall -Wextra -Wpedantic")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ALL_FLAGS}")

set(EXECUTABLE zejfmeteo_beta_1.0.0)

# Project configuration
project(zejfmeteo)

include_directories(lib/zejf_api/inc)

add_subdirectory(src)
add_subdirectory(lib)

target_include_directories(zejf_api PUBLIC src)

target_link_libraries(zejfmeteo zejf_api)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${EXECUTABLE})


