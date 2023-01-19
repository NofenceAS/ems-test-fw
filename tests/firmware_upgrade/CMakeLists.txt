#
# Copyright (c) 2022 Nofence AS
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(firmware_upgrade_testing)

zephyr_include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

FILE(GLOB app_sources
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/mock/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/mock/net/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/mock/power/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/fw_upgrade/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/error_handler/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/power_manager/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/fw_upgrade/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/diagnostics/diagnostics_events.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/diagnostics/diagnostic_flags.c      
)
target_sources(app PRIVATE ${app_sources})
target_include_directories(app PRIVATE ${APP_SOURCE})

zephyr_library_include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/mock/
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/fw_upgrade
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/fw_upgrade
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/power_manager/
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/error_handler/
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/diagnostics
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/diagnostics    
)