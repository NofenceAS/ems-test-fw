zephyr_include_directories(
        .
        ../../../include
)

target_sources(app PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/amc_events.c
        )
