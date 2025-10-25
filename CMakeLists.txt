cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

set(PICO_BOARD pico2)
set(PICO_PLATFORM rp2350)
set(ENV{PICO_SDK_PATH} "/home/leo/pico-sdk")
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
project(lcd_gauge C CXX ASM)



pico_sdk_init()

file(GLOB SRC_FILES CONFIGURE_DEPENDS
    src/*.c
)

add_executable(${PROJECT_NAME}
    ${SRC_FILES}
)

# Link with the Pico standard libraries
target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    pico_time
    hardware_spi
    hardware_dma
    hardware_irq
    hardware_gpio
    hardware_regs
)

# Enable USB and/or UART for stdio
pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)

# Create additional output formats (UF2, bin, etc.)
pico_add_extra_outputs(${PROJECT_NAME})