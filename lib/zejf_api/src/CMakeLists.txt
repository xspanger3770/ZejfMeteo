cmake_minimum_required(VERSION 3.13)

file(GLOB ZEJF_API_SOURCES "./*.c")

add_library(zejf_api ${ZEJF_API_SOURCES})

target_include_directories(zejf_api PUBLIC ../inc/)
