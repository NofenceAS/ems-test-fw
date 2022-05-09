#
# Copyright (c) 2022 Nofence AS
#

cmake_minimum_required(VERSION 3.20.0)
set(X3_PROJECT_BINARY "../../build")
list(APPEND ZEPHYR_EXTRA_MODULES
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/collar_protocol
        )

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(eeprom_testing)

zephyr_include_directories(
        ${CMAKE_CURRENT_BINARY_DIR}
        ${X3_PROJECT_BINARY}/include/generated/
)


FILE(GLOB app_sources 
        src/*.c 
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/nf_settings/*.c

)
target_sources(app PRIVATE ${app_sources})
target_include_directories(app PRIVATE ${APP_SOURCE})


zephyr_library_include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/nf_settings
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/nf_settings/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/movement_controller
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/modules/amc/lib/include
)

add_dependencies(app collar_protocol_headers)