#
# Copyright (c) 2022 Nofence AS
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(electric_pulse_testing)

zephyr_include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

FILE(GLOB app_sources 
        ${CMAKE_CURRENT_SOURCE_DIR}/mock/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/electric_pulse/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/electric_pulse/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/error_handler/error_event.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/buzzer/sound_event.c
)
target_sources(app PRIVATE ${app_sources})
target_include_directories(app PRIVATE ${APP_SOURCE})

zephyr_library_include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/mock
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/electric_pulse
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/electric_pulse
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/error_handler
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/buzzer
)