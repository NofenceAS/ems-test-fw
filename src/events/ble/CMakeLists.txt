#
# Copyright (c) 2021 Nofence
#

target_include_directories(app PRIVATE .)

target_sources(app PRIVATE
		     ${CMAKE_CURRENT_SOURCE_DIR}/ble_ctrl_event.c
		     ${CMAKE_CURRENT_SOURCE_DIR}/ble_data_event.c
		     ${CMAKE_CURRENT_SOURCE_DIR}/ble_conn_event.c
		     ${CMAKE_CURRENT_SOURCE_DIR}/ble_beacon_event.c
)
