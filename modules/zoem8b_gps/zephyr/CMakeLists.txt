# SPDX-License-Identifier: Apache-2.0

if (CONFIG_ZOEM8B_GPS)
    zephyr_include_directories(.)

    zephyr_library()
    zephyr_library_sources(
            zoem8b_gps.c
            ubx_controller.c
    )
endif ()
