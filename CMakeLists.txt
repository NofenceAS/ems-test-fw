# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
# Re-direct the directory where the 'boards' directory is found from
# $ZEPHYR_BASE to this directory.
#set(BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR})

# This sample is only compatible with nofence_x3_nrf52840 because it has
# re-directed the 'boards' directory to a directory that only contains
# this board.
set(BOARD nrf52840dk_nrf52840)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(x3_fw)


target_sources(app PRIVATE src/main.c
        src/ble/nf_ble.c
        src/ble/beacon_processor.c
        )

