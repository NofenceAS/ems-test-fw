
if(CONFIG_GNSS)
	zephyr_include_directories(.)

	zephyr_library()
	
	zephyr_library_sources(gnss_uart.c)

	if(CONFIG_GNSS_NMEA_PARSER)
		zephyr_library_sources(nmea_parser.c)
	endif()

	if(CONFIG_GNSS_UBLOX_MIA_M10)
		zephyr_library_sources(ublox-mia-m10.c)
		zephyr_library_sources(ublox_protocol.c)
	endif()
endif()