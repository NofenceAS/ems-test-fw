# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(protocol_testing)

zephyr_include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

FILE(GLOB app_sources src/*.c)

zephyr_library_include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/communication
)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/communication)

set(APP_SOURCE ../../src/modules/communication)
target_sources(app PRIVATE ${APP_SOURCE}/collar_protocol.c)
target_include_directories(app PRIVATE ${APP_SOURCE})
