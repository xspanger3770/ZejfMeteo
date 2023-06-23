cmake_minimum_required(VERSION 3.13)

add_library(bmp085 bmp085.c)

target_include_directories(bmp085 PUBLIC ../inc/)

target_link_libraries(bmp085 
    pico_stdlib
    hardware_i2c
)
