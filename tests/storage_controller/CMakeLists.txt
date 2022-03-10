#
# Copyright (c) 2022 Nofence AS
#

cmake_minimum_required(VERSION 3.20.0)

list(APPEND ZEPHYR_EXTRA_MODULES
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/collar_protocol
        )

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(storage_controller)

zephyr_include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

FILE(GLOB app_sources 
        src/*.c 
        mock/*.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/storage/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/storage_controller/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/storage_controller/fcb_ext/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/storage/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/error_handler/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/messaging/*.c
)
target_sources(app PRIVATE ${app_sources})
target_include_directories(app PRIVATE ${APP_SOURCE})

zephyr_library_include_directories(
  src/
  mock/
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/storage_controller/
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/storage_controller/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/storage_controller/fcb_ext
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/storage
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/error_handler
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/messaging
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/messaging
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)
add_dependencies(app collar_protocol_headers)