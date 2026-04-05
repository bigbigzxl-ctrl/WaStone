cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(stm32f103_bluepill_zephyr_hello_loop)
target_sources(app PRIVATE src/main.c)
