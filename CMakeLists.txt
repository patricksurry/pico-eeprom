# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)


# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(pico-eeprom)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1
add_executable(pico-eeprom
    pico-eeprom.c
)

pico_set_program_name(pico-eeprom "pico-eeprom")
pico_set_program_version(pico-eeprom "0.1")

# Generate PIO header
pico_generate_pio_header(pico-eeprom
    ${CMAKE_CURRENT_LIST_DIR}/blink.pio
)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(pico-eeprom 0)
pico_enable_stdio_usb(pico-eeprom 1)

# Add the standard library to the build
target_link_libraries(pico-eeprom
    pico_stdlib)

# Add any user requested libraries
target_link_libraries(pico-eeprom
    hardware_pio
)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(pico-eeprom)
