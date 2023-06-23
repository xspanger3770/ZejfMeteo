cmake_minimum_required(VERSION 3.13)

add_library(htu21 htu21.c)

target_include_directories(htu21 PUBLIC ../inc/)

target_link_libraries(htu21 
    pico_stdlib
    hardware_i2c
)
