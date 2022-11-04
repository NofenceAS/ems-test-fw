#
# Copyright (c) 2022 Nofence AS
#


cmake_minimum_required(VERSION 3.20.0)

list(APPEND ZEPHYR_EXTRA_MODULES
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/collar_protocol
        )

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(power_manager_tests)

zephyr_include_directories(
        ${CMAKE_CURRENT_BINARY_DIR}
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

FILE(GLOB app_sources 
        ${CMAKE_CURRENT_SOURCE_DIR}/mock/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/power_manager/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/power_manager/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/messaging/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/power_manager/battery/battery.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/error_handler/error_event.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/buzzer/sound_event.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/ble/ble_ctrl_event.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/watchdog/watchdog_event.c
)
target_sources(app PRIVATE ${app_sources})
target_include_directories(app PRIVATE ${APP_SOURCE})

zephyr_library_include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/mock
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/power_manager
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/power_manager
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/messaging
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/error_handler
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/buzzer
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/power_manager/battery/
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/ble
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/watchdog
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/storage_controller/
  #${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/nf_settings/include
)

add_dependencies(app collar_protocol_headers)
