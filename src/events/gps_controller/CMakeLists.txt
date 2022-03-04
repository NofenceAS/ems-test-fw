cmake_minimum_required(VERSION 3.20.0)

list(APPEND ZEPHYR_EXTRA_MODULES
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../src/drivers/gnss
        ${CMAKE_CURRENT_SOURCE_DIR}/mock
        )

zephyr_include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../src/drivers/gnss/zephyr
)

target_include_directories(app PRIVATE .)
target_sources(app PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/gnss_controller_events.c
        )