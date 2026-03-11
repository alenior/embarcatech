# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# == DO NOT EDIT (necessário para extensão do Pico) ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()

set(sdkVersion 1.5.1)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.0.0)

include(${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
# ====================================================

cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(bitdoglab_rgb_blink C CXX ASM)

pico_sdk_init()

add_executable(bitdoglab_rgb_blink
    main.c
)

target_link_libraries(bitdoglab_rgb_blink
    pico_stdlib
)

pico_enable_stdio_usb(bitdoglab_rgb_blink 1)
pico_enable_stdio_uart(bitdoglab_rgb_blink 0)

pico_add_extra_outputs(bitdoglab_rgb_blink)