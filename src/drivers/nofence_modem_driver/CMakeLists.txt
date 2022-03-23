# SPDX-License-Identifier: Apache-2.0
zephyr_include_directories(
	${ZEPHYR_BASE}/drivers/modem
	)
	
if(CONFIG_MODEM_UBLOX_SARA_NF)
target_sources(app PRIVATE
		ublox-sara-r4_nf.c
		)

#	zephyr_library_include_directories(${ZEPHYR_BASE}/subsys/net/ip)
#	zephyr_library_sources(ublox-sara-r4_nf.c)
endif()

