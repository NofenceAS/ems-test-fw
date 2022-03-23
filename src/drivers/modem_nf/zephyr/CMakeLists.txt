
if(CONFIG_MODEM_UBLOX_SARA_NF)
	zephyr_include_directories(.)
	zephyr_include_directories(${ZEPHYR_BASE}/drivers/modem)

	zephyr_library()
	
	zephyr_library_sources(ublox-sara-r4_nf.c)
endif()