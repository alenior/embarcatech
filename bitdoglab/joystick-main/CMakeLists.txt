# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 1.5.1)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.0.0)
include(${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
# ====================================================================================

cmake_minimum_required(VERSION 3.21)

include(pico_sdk_import.cmake)

project(joystick-main C CXX ASM)

pico_sdk_init()

add_executable(joystick-main
   joystick.c
)

pico_enable_stdio_uart(joystick-main 0)
pico_enable_stdio_usb(joystick-main 1)

target_link_libraries(joystick-main pico_stdlib hardware_adc)

pico_add_extra_outputs(joystick-main)