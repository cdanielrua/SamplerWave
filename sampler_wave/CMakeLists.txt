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
set(sdkVersion 1.5.1)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.0.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(sampler_wave C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(sampler_wave integrado_final.c)

pico_generate_pio_header(sampler_wave ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

pico_set_program_name(sampler_wave "sampler_wave")
pico_set_program_version(sampler_wave "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(sampler_wave 0)
pico_enable_stdio_usb(sampler_wave 1)

# Add the standard library to the build
target_link_libraries(sampler_wave
        pico_stdlib hardware_pwm hardware_dma hardware_adc hardware_irq hardware_pio pico_sync m hardware_clocks)

# Add the standard include files to the build
target_include_directories(sampler_wave PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(sampler_wave)

#target_link_libraries(sampler_wave m)