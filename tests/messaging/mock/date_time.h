/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _DATE_TIME_MOCK_H_
#define _DATE_TIME_MOCK_H_

#include <zephyr.h>
#include <stdint.h>
#include <time.h>

int date_time_set(struct tm *tm_date);

#endif /* _DATE_TIME_MOCK_H_ */