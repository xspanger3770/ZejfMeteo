cmake_minimum_required(VERSION 3.13)

add_library(ds3231 DS3231.h DS3231.cpp)

target_include_directories(ds3231 PUBLIC ./)

target_link_libraries(ds3231 
    pico_stdlib
    hardware_i2c
)
