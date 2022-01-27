#
# Copyright (c) 2022 Nofence AS
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(cellular_controller_testing)

zephyr_include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

FILE(GLOB app_sources 
        src/*.c 
        mock/*.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/cellular_controller/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/cellular_controller/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/messaging/*.c
)
target_sources(app PRIVATE ${app_sources})
target_include_directories(app PRIVATE ${APP_SOURCE})

zephyr_library_include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/mock
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/cellular_controller
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/cellular_controller/helpers
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/cellular_controller
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/messaging
)