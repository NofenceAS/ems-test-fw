#
# Copyright (c) 2021 Nofence
#
target_include_directories(app PRIVATE .)

target_sources(app PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/messaging_module_events.c
        ${CMAKE_CURRENT_SOURCE_DIR}/module_state_event.c
        ${CMAKE_CURRENT_SOURCE_DIR}/msg_data_event.c
)
zephyr_include_directories(
        ../../../include
)