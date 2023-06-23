cmake_minimum_required(VERSION 3.13)

add_library(tcp_handler tcp_handler.h tcp_handler.c)

target_include_directories(tcp_handler PUBLIC ./)

target_link_libraries(tcp_handler 
    pico_stdlib
    zejf_api
    pico_cyw43_arch_lwip_poll
)
