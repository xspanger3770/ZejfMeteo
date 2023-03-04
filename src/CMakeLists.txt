cmake_minimum_required(VERSION 3.13)

file(GLOB ZEJF_SOURCES "./*.c")

add_executable(zejfmeteo ${ZEJF_SOURCES})
