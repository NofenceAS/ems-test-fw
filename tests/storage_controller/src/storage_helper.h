/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _STORAGE_HELPER_H_
#define _STORAGE_HELPER_H_

#include <ztest.h>
#include "storage.h"
#include "storage_events.h"

void request_data(flash_partition_t partition);
void consume_data(flash_partition_t partition);
void write_data(flash_partition_t partition);

extern int consumed_entries_counter;

void test_pasture_write(void);
void test_pasture_read(void);

#endif /* _STORAGE_HELPER_H_ */