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
set(PICO_BOARD pico2_w CACHE STRING "Board type")


# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(JellyFloatOS C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(JellyFloatOS 
    JellyFloatOS.cpp
        jell_audio.cpp
        jell_led.hpp
        jell_pmw_light.hpp
        jell_canvas.cpp
        jell_effects.cpp
        jell_field.cpp
        jell_config.hpp
        jell_state.hpp
        jell_beat.cpp
        jell_net.cpp
        dhcpserver.c
)

pico_generate_pio_header(JellyFloatOS ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)
pico_generate_pio_header(JellyFloatOS ${CMAKE_CURRENT_LIST_DIR}/i2s_microphone.pio)

pico_set_program_name(JellyFloatOS "JellyFloatOS")
pico_set_program_version(JellyFloatOS "0.1.0")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(JellyFloatOS 0)
pico_enable_stdio_usb(JellyFloatOS 1)

# Add the standard library to the build
target_link_libraries(JellyFloatOS
        pico_stdlib
        hardware_pio
        pico_multicore
        hardware_pwm
        hardware_dma
        )

# Add the standard include files to the build
target_include_directories(JellyFloatOS PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# WLAN: CYW43 driver plus lwIP, run from a background IRQ on the core that calls
# cyw43_arch_init (core 0). lwipopts.h in this directory configures lwIP.
target_link_libraries(JellyFloatOS
        pico_cyw43_arch_lwip_threadsafe_background
        pico_unique_id
        pico_rand
        )

# Per-build overrides for hardware and network settings, so one source tree serves
# every jelly:   cmake ... -DJELL_RING_LEDS=39 -DJELL_WIFI_SSID=bloom -DJELL_WIFI_PASSWORD=secret123
# The CI workflow exposes the same three as inputs of its manual run.
if (DEFINED JELL_RING_LEDS AND NOT "${JELL_RING_LEDS}" STREQUAL "")
    target_compile_definitions(JellyFloatOS PRIVATE JELL_RING_LEDS=${JELL_RING_LEDS})
    message(STATUS "JellyFloatOS: ring with ${JELL_RING_LEDS} LEDs")
endif()
foreach(order_opt JELL_RING_ORDER JELL_TENTACLE_ORDER)
    if (DEFINED ${order_opt} AND NOT "${${order_opt}}" STREQUAL "")
        target_compile_definitions(JellyFloatOS PRIVATE ${order_opt}=${${order_opt}})
        message(STATUS "JellyFloatOS: ${order_opt} = ${${order_opt}}")
    endif()
endforeach()
if (DEFINED JELL_WIFI_SSID AND NOT "${JELL_WIFI_SSID}" STREQUAL "")
    target_compile_definitions(JellyFloatOS PRIVATE "JELL_WIFI_SSID=\"${JELL_WIFI_SSID}\"")
    message(STATUS "JellyFloatOS: network name ${JELL_WIFI_SSID}")
endif()
if (DEFINED JELL_WIFI_PASSWORD AND NOT "${JELL_WIFI_PASSWORD}" STREQUAL "")
    target_compile_definitions(JellyFloatOS PRIVATE "JELL_WIFI_PASSWORD=\"${JELL_WIFI_PASSWORD}\"")
endif()

pico_add_extra_outputs(JellyFloatOS)


