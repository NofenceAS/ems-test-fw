#
# Copyright (c) 2022 Nofence AS
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(gnss_controller_testing)

zephyr_include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../src/drivers/gnss/zephyr
        )

FILE(GLOB app_sources
        src/*.c
        mock/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/gnss_controller/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/gnss_controller/*.c
        )
target_sources(app PRIVATE ${app_sources})
target_include_directories(app PRIVATE ${APP_SOURCE})

zephyr_library_include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/mock
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/gnss_controller
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/gnss_controller
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)