#
# Copyright (c) 2022 Nofence AS
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(env_sensor_test)

zephyr_include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

FILE(GLOB app_sources 
        ${CMAKE_CURRENT_SOURCE_DIR}/mock/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/mock/drivers/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/environment/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/environment/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/error_handler/*.c
)
target_sources(app PRIVATE ${app_sources})
target_include_directories(app PRIVATE ${APP_SOURCE})

zephyr_library_include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/mock
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/environment/
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/environment
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/error_handler/
)