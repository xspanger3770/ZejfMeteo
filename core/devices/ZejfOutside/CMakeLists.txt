cmake_minimum_required(VERSION 3.13)

#set(CMAKE_C_STANDARD 11)
#set(CMAKE_CXX_STANDARD 17)

set(ALL_FLAGS "-O2 -Wall -Wextra -Werror -Wno-psabi")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ALL_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ALL_FLAGS}")

include(import/pico_sdk_import.cmake)

project(zejf_outside C CXX ASM)

pico_sdk_init()

add_subdirectory(../../api/zejf_api zejf_api)
add_subdirectory(../../libs/ libs)

target_include_directories(zejf_api PUBLIC src)

file(GLOB MY_SOURCES "src/*.c*")

add_executable(zejf_outside ${MY_SOURCES})

target_compile_definitions(zejf_outside PRIVATE PICO_MALLOC_PANIC=0)

target_link_libraries(zejf_outside 
    pico_stdlib
    zejf_api
    zejf_pico_utils
    hardware_rtc
    hardware_adc
    tcp_handler
    htu21
)

pico_set_program_name(zejf_outside "zejf_outside")
pico_set_program_version(zejf_outside "0.0.0")

pico_enable_stdio_uart(zejf_outside 0)
pico_enable_stdio_usb(zejf_outside 1)

pico_add_extra_outputs(zejf_outside)