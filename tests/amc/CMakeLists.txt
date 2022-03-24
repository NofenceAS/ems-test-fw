#
# Copyright (c) 2022 Nofence AS
#

cmake_minimum_required(VERSION 3.20.0)

list(APPEND ZEPHYR_EXTRA_MODULES
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/collar_protocol
        )

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(protocol_testing)

zephyr_include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

FILE(GLOB app_sources
        src/*.c 
        mock/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/lib/trigonometry/trigonometry.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/amc/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/amc/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/amc/lib/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/buzzer/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/error_handler/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/messaging/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/storage_controller/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/gnss_controller/*.c
)
target_sources(app PRIVATE ${app_sources})
target_include_directories(app PRIVATE ${APP_SOURCE})

zephyr_library_include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/mock
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include/
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/lib/trigonometry/
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/amc/
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/amc/lib/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/amc
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/buzzer
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/error_handler
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/storage_controller
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/gnss_controller
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/messaging
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/storage_controller/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/storage_controller
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/drivers/gnss/zephyr
)

add_dependencies(app collar_protocol_headers)
