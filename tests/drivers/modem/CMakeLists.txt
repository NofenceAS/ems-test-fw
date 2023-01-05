# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)



list(APPEND ZEPHYR_EXTRA_MODULES
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../src/drivers/modem_nf
        ${CMAKE_CURRENT_SOURCE_DIR}/../mock
        )

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(modem_testing)

zephyr_include_directories(${CMAKE_CURRENT_BINARY_DIR}
                            ${CMAKE_CURRENT_BINARY_DIR}/mock)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../include
                    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/drivers/modem_nf/zephyr
                    ${CMAKE_CURRENT_SOURCE_DIR}/mock
                    ${CMAKE_CURRENT_SOURCE_DIR}/src)

FILE(GLOB app_sources 
    src/*.c
    mock/*.c)
target_sources(app PRIVATE ${app_sources})
target_include_directories(app PRIVATE ${APP_SOURCE})

