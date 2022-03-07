/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _STORAGE_H_
#define _STORAGE_H_

#include <zephyr.h>

typedef int (*stg_read_log_cb)(uint8_t *data, size_t len);

int stg_read_pasture_data(stg_read_log_cb cb);

#endif /* _STORAGE_H_ */