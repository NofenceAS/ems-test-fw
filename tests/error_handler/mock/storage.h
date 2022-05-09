/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _STORAGE_H_
#define _STORAGE_H_

#include <zephyr.h>

int stg_write_system_diagnostic_log(uint8_t *data, size_t len);

#endif /* _STORAGE_H_ */