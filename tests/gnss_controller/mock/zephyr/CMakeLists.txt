
if(CONFIG_SERIAL_MOCK)
	zephyr_include_directories(.)

	zephyr_library()
	
	zephyr_library_sources(mock_gnss.c)
endif()