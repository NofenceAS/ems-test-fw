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
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/collar_stats/
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/power_manager/
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/environment/
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/movement_controller/
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/amc/lib/include/
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/time_use/lib/include/
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/lib/crc
		${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/error_handler

)

FILE(GLOB app_sources 
        ${CMAKE_CURRENT_SOURCE_DIR}/mock/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
		${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/time_use/time_use.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/lib/crc/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/ble/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/buzzer/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/lte/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/messaging/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/messaging/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/cellular_controller/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/gnss_controller/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/error_handler/error_event.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/storage/storage_event.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/fw_upgrade/fw_upgrade_events.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/power_manager/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/environment/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/collar_stats/histogram_events.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/time_use/libs/helpers.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/amc/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/movement_controller/*.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../src/events/watchdog/watchdog_event.c
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
