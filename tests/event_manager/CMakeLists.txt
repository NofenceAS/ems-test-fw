zephyr_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/events
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules)
target_sources(app PRIVATE 
${CMAKE_CURRENT_SOURCE_DIR}/src/modules/listener.c
${CMAKE_CURRENT_SOURCE_DIR}/src/modules/publisher.c
${CMAKE_CURRENT_SOURCE_DIR}/src/events/req_event.c
${CMAKE_CURRENT_SOURCE_DIR}/src/events/sensor_event.c
${CMAKE_CURRENT_SOURCE_DIR}/test/event_manager_test.c
)