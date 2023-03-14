cmake_minimum_required(VERSION 3.13)

# initialize pico-sdk from submodule
# note: this must happen before project()
include(pico-sdk/pico_sdk_init.cmake)

project(Plick)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# rest of your project
add_executable(main
    main.cpp
    hw_config.c
)

add_subdirectory(
    no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI build
)

target_sources(main PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/main.cpp
        ${CMAKE_CURRENT_LIST_DIR}/usb_descriptors.c
        )

target_include_directories(main PUBLIC
        ${CMAKE_CURRENT_LIST_DIR})


# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(main PUBLIC
    pico_stdlib
    pico_cyw43_arch_none
    hardware_adc
    FatFs_SPI
    tinyusb_device 
    tinyusb_board
    )

# enable and disble usb uart
pico_enable_stdio_usb(main 1)
pico_enable_stdio_uart(main 0)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(main)