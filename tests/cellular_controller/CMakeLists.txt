#
# Copyright (c) 2022 Nofence AS
#

cmake_minimum_required(VERSION 3.20.0)

list(APPEND ZEPHYR_EXTRA_MODULES
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/collar_protocol
        )

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(cellular_controller_testing)

zephyr_include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

FILE(GLOB app_sources
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/mock/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/cellular_controller/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/cellular_controller/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/messaging/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/error_handler/error_event.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/fw_upgrade/fw_upgrade_events.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/power_manager/pwr_event.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/diagnostics/diagnostics_events.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/diagnostics/diagnostic_flags.c
        )
target_sources(app PRIVATE ${app_sources})
target_include_directories(app PRIVATE ${APP_SOURCE})

zephyr_library_include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/mock
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/drivers/modem_nf/zephyr
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/cellular_controller
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/collar_protocol/zephyr
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/cellular_controller/helpers
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/cellular_controller
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/messaging
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/fw_upgrade
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/power_manager
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/diagnostics
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/storage_controller/
        #${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/nf_settings/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/error_handler
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/drivers/modem_nf/zephyr
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/drivers/gnss/zephyr
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/diagnostics
)
add_dependencies(app collar_protocol_headers)
