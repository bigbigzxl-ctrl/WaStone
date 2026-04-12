cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nrf52840_nicenano_wire_check)
target_sources(app PRIVATE src/main.c ../nrf52840_nicenano_common/ael_usb.c)
