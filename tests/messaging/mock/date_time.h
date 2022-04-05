/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _DATE_TIME_MOCK_H_
#define _DATE_TIME_MOCK_H_

int date_time_set(int64_t unixtime);
int date_time_now(int64_t *unixtime);

#endif /* _DATE_TIME_MOCK_H_ */