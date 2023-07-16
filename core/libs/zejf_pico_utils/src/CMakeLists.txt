cmake_minimum_required(VERSION 3.13)

add_library(zejf_pico_utils hw_config.cpp sd_card_handler.cpp time_utils.cpp time_engine.cpp zejf_runtime.cpp zejf_pico_data.cpp)

target_include_directories(zejf_pico_utils PUBLIC ../inc/)

target_link_libraries(zejf_pico_utils 
    pico_stdlib
    FatFs_SPI
    zejf_api
    ds3231
    hardware_adc
    hardware_rtc
    tcp_handler
)
