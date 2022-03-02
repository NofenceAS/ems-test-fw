# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bluetooth)

zephyr_include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

FILE(GLOB app_sources 
        src/*.c 
        mock/*.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/ble/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/messaging/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/ble/ble_controller.c
)
target_sources(app PRIVATE ${app_sources})
target_include_directories(app PRIVATE ${APP_SOURCE})

zephyr_library_include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/mock/
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include/
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/ble
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/ble
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/messaging
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/messaging
)


#target_sources(app PRIVATE src/bluetooth.c)
