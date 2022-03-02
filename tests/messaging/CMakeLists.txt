#
# Copyright (c) 2022 Nofence AS
#

cmake_minimum_required(VERSION 3.20.0)
set(X3_PROJECT_BINARY "../../build")
list(APPEND ZEPHYR_EXTRA_MODULES
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/collar_protocol
        )

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(messaging_testing)

zephyr_include_directories(
        ${CMAKE_CURRENT_BINARY_DIR}
        ${X3_PROJECT_BINARY}/include/generated/
)

FILE(GLOB app_sources 
        mock/*.c
        src/*.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/ble/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/lte/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/messaging/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/messaging/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/cellular_controller/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/gps_controller/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/error_handler/error_event.c

)
target_sources(app PRIVATE ${app_sources})
target_include_directories(app PRIVATE ${APP_SOURCE})

zephyr_library_include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/mock
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/messaging/
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/messaging/
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/ble
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/lte
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/cellular_controller
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/gps_controller
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/amc
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/error_handler
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)


add_dependencies(app collar_protocol_headers)
