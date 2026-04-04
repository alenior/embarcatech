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

project(microfone C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# ── Arquivos-fonte do projeto ────────────────────────────────
# add_executable recebe APENAS arquivos .c / .h do seu projeto,
# nunca nomes de bibliotecas do SDK.
add_executable(microfone
    main.c
    ssd1306.c
    ssd1306.h
)

pico_set_program_name(microfone "microfone")
pico_set_program_version(microfone "0.1")

# Saída serial
pico_enable_stdio_uart(microfone 0)
pico_enable_stdio_usb(microfone 1)

# ── Diretório de include (para encontrar ssd1306.h) ──────────
target_include_directories(microfone PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

# ── Bibliotecas do SDK ───────────────────────────────────────
# target_link_libraries recebe os NOMES das bibliotecas CMake do
# pico-sdk, sem extensão e sem caminho — nunca nomes de headers.
target_link_libraries(microfone
    pico_stdlib                       # base: GPIO, sleep, stdio
    pico_cyw43_arch_lwip_poll         # Wi-Fi + lwIP (modo polling, sem RTOS)
    hardware_adc                      # leitura analógica (microfone)
    hardware_dma                      # captura DMA contínua do ADC
    hardware_timer                    # timestamps (to_us_since_boot)
    hardware_i2c                      # display OLED SSD1306
    m                                 # libm: sqrt() em calculate_rms
)

# ── Artefatos de saída (.uf2, .hex, .bin, .map) ─────────────
pico_add_extra_outputs(microfone)