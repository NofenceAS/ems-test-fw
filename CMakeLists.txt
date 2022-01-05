# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Print out build type
message(STATUS "Build type: ${BUILD_TYPE} 🚀")

# Define configuration files.
if (HARDWARE_TEST)
list(APPEND CONF_FILE
        ${CMAKE_CURRENT_SOURCE_DIR}/prj.conf
        ${CMAKE_CURRENT_SOURCE_DIR}/hw_test.conf
        )
endif()

# Out-of-tree driver for GNSS,
# see https://github.com/zephyrproject-rtos/zephyr/tree/main/samples/application_development/out_of_tree_driver
list(APPEND ZEPHYR_EXTRA_MODULES
        )
list(APPEND ZEPHYR_EXTRA_MODULES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/collar_protocol/
        )

# Re-direct the directory where the 'boards' directory is found from
# $ZEPHYR_BASE to this directory.
set(BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR})

# The Nofence X3 board exists out-of-tree from zephyr boards
set(BOARD nf_x25_nrf52840)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(x3_fw)

# As the Zephyr GNSS API is not yet finalized, ( see https://github.com/zephyrproject-rtos/zephyr/issues/30564)
# We put the proposed API out-of-tree
zephyr_include_directories(include
                          )

zephyr_include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# These are common files used by both hardware test and the real app
set(APP_SOURCES
        src/ble/nf_ble.c        
        src/ble/beacon_processor.c
        )

add_dependencies(app
        		collar_protocol_headers
    )

if (HARDWARE_TEST)
    set(APP_MAIN src/main_hardware_test.c)
else ()
    set(APP_MAIN src/main.c)
endif ()

target_sources(app PRIVATE ${APP_MAIN}
        ${APP_SOURCES}
        )

# Application sources
#add_subdirectory(src/events/ble)
#add_subdirectory(src/modules/ble)

add_subdirectory(src/events/messaging)
add_subdirectory(src/modules/messaging)
