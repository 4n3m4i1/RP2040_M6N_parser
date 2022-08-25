cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)

project(gpsRead C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_EXAMPLES_PATH ${PROJECT_SOURCE_DIR})

pico_sdk_init()

add_executable(gpsRead
	main.c
	)

# Link pico/stdlib.h
target_link_libraries(gpsRead pico_stdlib)

pico_enable_stdio_usb(gpsRead 1)
pico_enable_stdio_uart(gpsRead 0)

# Create proper binaries
pico_add_extra_outputs(gpsRead)



