#
# Copyright (c) 2022 Nofence AS
#

cmake_minimum_required(VERSION 3.20.0)

list(APPEND ZEPHYR_EXTRA_MODULES
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/drivers/gnss
        ${CMAKE_CURRENT_SOURCE_DIR}/mock
        )

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(gnss_controller_testing)

zephyr_include_directories(${CMAKE_CURRENT_BINARY_DIR})

zephyr_library_include_directories(
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/mock
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/gnss_controller
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/gnss_controller
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/error_handler
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/drivers/gnss/zephyr
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/nf_settings/include
)

FILE(GLOB app_sources
        src/*.c
        mock/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/gnss_controller/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/error_handler/error_event.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/gnss_controller/*.c
        )
target_sources(app PRIVATE ${app_sources})
target_include_directories(app PRIVATE ${APP_SOURCE})

