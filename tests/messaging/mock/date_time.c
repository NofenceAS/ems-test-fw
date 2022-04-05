/*
 * Copyright (c) 2022 Nofence AS
 */
#include <ztest.h>
#include <time.h>

int date_time_set(struct tm *tm_date)
{
	return ztest_get_return_value();
}