/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _STORAGE_H_
#define _STORAGE_H_

#include <zephyr.h>

typedef int (*fcb_read_cb)(uint8_t *data, size_t len);

int stg_read_pasture_data(fcb_read_cb cb);

#endif /* _STORAGE_H_ */