
if(CONFIG_GNSS_CONTROLLER)
        zephyr_include_directories(
                .
                ./../../events/gnss_controller
                ./../../../src/events/error_handler
                ./../../drivers/gnss/zephyr
                ./../../modules/nf_settings/include
                ./../../events/diagnostics/
                )

        target_sources(app PRIVATE
                gnss_controller.c
                ./../../events/gnss_controller/gnss_controller_events.c
                ./../../events/error_handler/error_event.c
                ./../../events/diagnostics/diagnostics_events.c
                )
endif()
