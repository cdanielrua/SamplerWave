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
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(dma_mic C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(dma_mic dma_mic.c )

pico_set_program_name(dma_mic "dma_mic")
pico_set_program_version(dma_mic "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(dma_mic 0)
pico_enable_stdio_usb(dma_mic 1)

# Add the standard library to the build
target_link_libraries(dma_mic
        hardware_dma
        hardware_pwm
        hardware_gpio
        hardware_irq
        hardware_timer
        hardware_adc
        hardware_clocks
        hardware_sync
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(dma_mic PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(dma_mic)

