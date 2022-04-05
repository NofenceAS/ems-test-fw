/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>

int date_time_now(int64_t *unixtime)
{
	/* Populate with fixed time which is used for ANO comparison. 
	 * Test time is set to GMT Tuesday, April 5, 2022 9:18:02 AM
	 */
	*unixtime = 1649150282;

	return ztest_get_return_value();
}