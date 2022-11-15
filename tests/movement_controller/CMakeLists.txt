#
# Copyright (c) 2022 Nofence AS
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(movement_controller_tests)

zephyr_include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

FILE(GLOB app_sources 
        ${CMAKE_CURRENT_SOURCE_DIR}/mock/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/mock/drivers/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/lib/fifo/*.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/lib/trigonometry/*.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/movement_controller/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/movement_controller/*.c
)

target_sources(app PRIVATE ${app_sources})
target_include_directories(app PRIVATE ${APP_SOURCE})

zephyr_library_include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/mock
  ${CMAKE_CURRENT_SOURCE_DIR}/mock/drivers
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/lib/fifo
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/lib/trigonometry
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/storage_controller/
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/movement_controller/
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/movement_controller/
)