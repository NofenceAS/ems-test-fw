/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _DATE_TIME_MOCK_H_
#define _DATE_TIME_MOCK_H_

#include <zephyr.h>
#include <stdint.h>

int date_time_now(int64_t *unixtime);

#endif /* _DATE_TIME_MOCK_H_ */