cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(ALL_FLAGS "-O2 -Wall -Wextra -Werror")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ALL_FLAGS}")

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ALL_FLAGS}")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ALL_FLAGS}")

include(import/pico_sdk_import.cmake)

project(zejf_press C CXX ASM)

pico_sdk_init()

add_subdirectory(../../api/zejf_api build/zejf_api)
add_subdirectory(../../libs/bmp390 build/libs/bmp390)
add_subdirectory(../../libs/zejf_pico_utils build/libs/zejf_pico_utils)
add_subdirectory(../../libs/FatFs_SPI build/libs/FatFs_SPI)

target_include_directories(zejf_api PUBLIC src)

file(GLOB MY_SOURCES "src/*.c*")

add_executable(zejf_press ${MY_SOURCES})

target_link_libraries(zejf_press 
    pico_stdlib
    hardware_i2c
    hardware_rtc
    zejf_api
    zejf_pico_utils
    bmp390
)

pico_set_program_name(zejf_press "zejf_press")
pico_set_program_version(zejf_press "0.0.0")

pico_enable_stdio_uart(zejf_press 0)
pico_enable_stdio_usb(zejf_press 1)

pico_add_extra_outputs(zejf_press)