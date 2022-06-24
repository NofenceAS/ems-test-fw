/*
 * Copyright (c) 2022 Nofence AS
 */
#include <ztest.h>
#include <time.h>

int date_time_set(struct tm *tm_date)
{
	return ztest_get_return_value();
}

int date_time_now(int64_t *unix_time_ms)
{
	return ztest_get_return_value();
}