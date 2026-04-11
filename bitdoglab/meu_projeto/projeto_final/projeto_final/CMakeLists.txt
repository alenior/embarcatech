# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

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
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(projeto_final C CXX ASM)

set(PICO_CYW43_ARCH_THREADSAFE_BACKGROUND 0)
set(PICO_CYW43_ARCH_POLL 1)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

include_directories(include)

# Add executable. Default name is the project name, version 0.1

add_executable(projeto_final 
    src/projeto_final.c
    src/wifi.c
    src/firebase.c
    src/display.c
    src/alarm.c
    src/pir.c
    src/audio.c
    src/buttons.c
    src/joystick.c
    src/utils.c
    src/ssd1306.c
    src/mbedtls_compat.c
    src/control.c
    src/time_sync.c
)

pico_set_program_name(projeto_final "projeto_final")
pico_set_program_version(projeto_final "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(projeto_final 0)
pico_enable_stdio_usb(projeto_final 1)

# Add the standard library to the build
target_link_libraries(projeto_final
        pico_stdlib
        hardware_adc
        hardware_dma
        hardware_sync
        hardware_i2c
        pico_cyw43_arch_lwip_poll
        pico_lwip_mbedtls
        pico_mbedtls
        hardware_watchdog
        hardware_gpio
)

# Add the standard include files to the build
target_include_directories(projeto_final PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

pico_add_extra_outputs(projeto_final)