
if(CONFIG_GNSS_DRIVER)
	zephyr_include_directories(.)

	zephyr_library()
	zephyr_library_sources(
		gnss.c
	)
endif()

if(CONFIG_GNSS_UBLOX_MIA_M10)
	zephyr_library_sources(ublox-mia-m10.c)
endif()