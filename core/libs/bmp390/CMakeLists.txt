cmake_minimum_required(VERSION 3.13)

add_library(bmp390 bmp390.h)

target_include_directories(bmp390 PUBLIC ./)

target_link_libraries(bmp390 
    pico_stdlib
    hardware_i2c
)
