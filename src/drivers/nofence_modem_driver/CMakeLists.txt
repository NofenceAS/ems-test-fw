target_sources(app PRIVATE
		ublox-sara-r4_nf.c
		)

#if(CONFIG_MODEM_UBLOX_SARA_NF)
#	zephyr_library_include_directories(${ZEPHYR_BASE}/subsys/net/ip)
#	zephyr_library_sources(ublox-sara-r4_nf.c)
#endif()

==== BASE ====
