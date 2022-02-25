# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Print out build type
message(STATUS "Build type: ${BUILD_TYPE} 🚀")

list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# Define configuration files.
#if (HARDWARE_TEST)
#list(APPEND CONF_FILE
#        ${CMAKE_CURRENT_SOURCE_DIR}/prj.conf
#        ${CMAKE_CURRENT_SOURCE_DIR}/hw_test.conf
#        )
#endif()



if (HARDWARE_TEST)
        if(DTC_OVERLAY_FILE)
                set(DTC_OVERLAY_FILE
                        "${DTC_OVERLAY_FILE};boards/arm/nf_x25_nrf52840/nf_x25_hw_test.overlay"
                        CACHE STRING "" FORCE
                        )
        else()
                set(DTC_OVERLAY_FILE boards/arm/nf_x25_nrf52840/nf_x25_hw_test.overlay)
        endif()
endif()

# Out-of-tree driver for GNSS,
# see https://github.com/zephyrproject-rtos/zephyr/tree/main/samples/application_development/out_of_tree_driver
list(APPEND ZEPHYR_EXTRA_MODULES
        )

list(APPEND ZEPHYR_EXTRA_MODULES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/collar_protocol
        )



# Re-direct the directory where the 'boards' directory is found from
# $ZEPHYR_BASE to this directory.
set(BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR})

# The Nofence X3 board exists out-of-tree from zephyr boards
set(BOARD nf_x25_nrf52840)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

include(cmake/version.cmake)

project(x3_fw)

# Genrerate firmware version info
configure_file(version.h.in ${PROJECT_BINARY_DIR}/include/generated/nf_version.h)
configure_file(version.sh.in ${PROJECT_BINARY_DIR}/nf_version.sh)

# As the Zephyr GNSS API is not yet finalized, ( see https://github.com/zephyrproject-rtos/zephyr/issues/30564)
# We put the proposed API out-of-tree
zephyr_include_directories(include
                          )
zephyr_include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

zephyr_include_directories(${PROJECT_BINARY_DIR}/include/generated)

#if (HARDWARE_TEST)
#        set(APP_MAIN src/main_hardware_test.c)
#else ()
        set(APP_MAIN src/main.c)
#endif ()

if (CONFIG_COLLAR_PROTOCOL)
        add_dependencies(app collar_protocol_headers)
endif ()

# Added as zephyr library
zephyr_library_include_directories(
        src/modules/amc
        src/modules/fw_upgrade
  	src/modules/storage_controller
        src/events
        src/modules/error_handler
        src/events/error_handler
        src/modules/diagnostics
        src/events/diagnostics
        src/events/fw_upgrade
	src/modules/cellular_controller
        src/events/cellular_controller
        ${PROJECT_BINARY_DIR}/include/generated/)
        
target_sources(app PRIVATE ${APP_MAIN}
        ${APP_SOURCES}
)       
        
# Application sources
add_subdirectory(src/events/ble)
add_subdirectory(src/modules/ble)
add_subdirectory(src/modules/fw_upgrade)
add_subdirectory(src/modules/storage_controller)
add_subdirectory(src/eeprom)
add_subdirectory(src/modules/amc)
add_subdirectory(src/modules/cellular_controller)

add_subdirectory(src/events/messaging)
add_subdirectory(src/modules/messaging)
add_subdirectory(src/modules/diagnostics)

add_subdirectory(src/events/electric_pulse)
add_subdirectory(src/modules/electric_pulse)
add_subdirectory(src//drivers/nofence_modem_driver)
