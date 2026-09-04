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
        jell_web.cpp
        jell_findmy.cpp
        dhcpserver.c
)

pico_generate_pio_header(JellyFloatOS ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)
pico_generate_pio_header(JellyFloatOS ${CMAKE_CURRENT_LIST_DIR}/i2s_microphone.pio)

# Version: the VERSION file at the repository root is the single source, shared with the
# app. Override with -DJELL_VERSION=... only for experiments. The git revision is taken
# from the working tree, or from -DJELL_GIT_REV=... (the CI passes it in).
if (NOT DEFINED JELL_VERSION OR "${JELL_VERSION}" STREQUAL "")
    file(READ "${CMAKE_CURRENT_LIST_DIR}/../VERSION" JELL_VERSION)
    string(STRIP "${JELL_VERSION}" JELL_VERSION)
endif()
if (NOT DEFINED JELL_GIT_REV OR "${JELL_GIT_REV}" STREQUAL "")
    execute_process(COMMAND git rev-parse --short HEAD
                    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
                    OUTPUT_VARIABLE JELL_GIT_REV OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if ("${JELL_GIT_REV}" STREQUAL "")
        set(JELL_GIT_REV "unknown")
    endif()
endif()
message(STATUS "JellyFloatOS: version ${JELL_VERSION} (${JELL_GIT_REV})")
target_compile_definitions(JellyFloatOS PRIVATE "JELL_VERSION=\"${JELL_VERSION}\"" "JELL_GIT_REV=\"${JELL_GIT_REV}\"")

pico_set_program_name(JellyFloatOS "JellyFloatOS")
pico_set_program_version(JellyFloatOS "${JELL_VERSION}")

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
        pico_lwip_http
        pico_unique_id
        pico_rand
        )

# Find My: BLE advertising through BTstack (btstack_config.h in this directory), the key in a
# flash sector of its own, and a linker script that keeps the program out of the last four
# sectors: the key's (fourth from the end), BTstack's two, and the last one the SDK leaves free.
target_link_libraries(JellyFloatOS
        pico_btstack_ble
        pico_btstack_cyw43
        pico_flash
        hardware_flash
        )
if ("${PICO_FLASH_SIZE_BYTES}" STREQUAL "")
    set(PICO_FLASH_SIZE_BYTES "(4 * 1024 * 1024)")
endif()
file(READ "${PICO_SDK_PATH}/src/rp2_common/pico_crt0/rp2350/memmap_default.ld" JELL_LD)
string(REPLACE "INCLUDE \"pico_flash_region.ld\""
       "FLASH(rx) : ORIGIN = 0x10000000, LENGTH = (${PICO_FLASH_SIZE_BYTES} - 4 * 4096)" JELL_LD "${JELL_LD}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/memmap_jelly.ld" "${JELL_LD}")
pico_set_linker_script(JellyFloatOS "${CMAKE_CURRENT_BINARY_DIR}/memmap_jelly.ld")

# The factory-reset image: erases the Find My sector and nothing else. A separate UF2,
# never part of a normal update; see reset/JellyFloatReset.cpp.
add_executable(JellyFloatReset reset/JellyFloatReset.cpp)
pico_set_program_name(JellyFloatReset "JellyFloatReset")
pico_set_program_version(JellyFloatReset "${JELL_VERSION}")
pico_enable_stdio_usb(JellyFloatReset 1)
target_link_libraries(JellyFloatReset pico_stdlib hardware_flash pico_cyw43_arch_none)
target_include_directories(JellyFloatReset PRIVATE ${CMAKE_CURRENT_LIST_DIR})
pico_add_extra_outputs(JellyFloatReset)

# The web page: everything under web/ is compiled into the firmware (pico_fsdata.inc)
# and served by lwIP's httpd, see jell_web.cpp.
file(GLOB JELL_WEB_FILES ${CMAKE_CURRENT_LIST_DIR}/web/*)
pico_set_lwip_httpd_content(JellyFloatOS PRIVATE ${JELL_WEB_FILES})

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


