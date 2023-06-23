cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(ALL_FLAGS "-O2 -Wall -Wextra -Werror")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ALL_FLAGS}")

include(import/pico_sdk_import.cmake)

project(zejf_wind C CXX ASM)

pico_sdk_init()

add_subdirectory(../../api/zejf_api build/zejf_api)
add_subdirectory(../../libs/ build/libs)

target_include_directories(zejf_api PUBLIC src)

file(GLOB MY_SOURCES "src/*.c*")

add_executable(zejf_wind ${MY_SOURCES})

target_link_libraries(zejf_wind 
    pico_stdlib
    zejf_api
    zejf_pico_utils
    hardware_rtc
    hardware_adc
    bmp085
    tcp_handler
)

pico_set_program_name(zejf_wind "zejf_wind")
pico_set_program_version(zejf_wind "0.0.0")

pico_enable_stdio_uart(zejf_wind 0)
pico_enable_stdio_usb(zejf_wind 1)

pico_add_extra_outputs(zejf_wind)