#
# Copyright (c) 2022 Nofence AS
#

cmake_minimum_required(VERSION 3.20.0)
set(X3_PROJECT_BINARY "../../build")
list(APPEND ZEPHYR_EXTRA_MODULES
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/collar_protocol
        )

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(electric_pulse_testing)

zephyr_include_directories(
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/mock
        ${CMAKE_CURRENT_SOURCE_DIR}/mock/drivers
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/electric_pulse
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/electric_pulse
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/error_handler
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/messaging
)

FILE(GLOB app_sources
        ${CMAKE_CURRENT_SOURCE_DIR}/mock/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/mock/drivers/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/electric_pulse/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/messaging/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/electric_pulse/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/error_handler/error_event.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/buzzer/sound_event.c
        )
target_sources(app PRIVATE ${app_sources})
target_include_directories(app PRIVATE ${APP_SOURCE})

zephyr_library_include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/mock
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/lib/crc/
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/messaging/
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/ble/services
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/buzzer
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/messaging/
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/ble
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/lte
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/cellular_controller
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/gnss_controller
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/amc
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/error_handler
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/storage/
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/storage_controller/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/fw_upgrade/
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/power_manager/
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/watchdog
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/nf_settings/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/drivers/gnss/zephyr
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/drivers/modem_nf/zephyr
)

add_dependencies(app collar_protocol_headers)