
if (CONFIG_DIAGNOSTICS)
zephyr_include_directories(
    .
    ./commander
    ./../../drivers/gnss/zephyr
    ./../../events/diagnostics/
    ./../../modules/buzzer
    ./../../modules/storage_controller
)

target_sources(app PRIVATE 
    diagnostics.c
    selftest.c
    onboard_data.c
    diagnostic_flags.c
    parser.c
    passthrough.c
    ./../../events/diagnostics/diagnostics_events.c
    ./../../modules/storage_controller/stg_config.c
    log_backend_diag.c
)

if (CONFIG_DIAGNOSTICS_TEXT_PARSER)
target_sources(app PRIVATE parser.c)
else()
target_sources(app PRIVATE 
    ./commander/commander.c
    ./commander/cobs.c
    ./commander/cmd_settings.c
    ./commander/cmd_stimulator.c
    ./commander/cmd_storage.c
    ./commander/cmd_system.c
    ./commander/cmd_modem.c
)
endif()

endif()
