/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _STORAGE_HELPER_H_
#define _STORAGE_HELPER_H_

#include <ztest.h>
#include "storage.h"
#include "storage_events.h"
#include "flash_memory.h"

extern mem_rec cached_mem_rec;

void request_data(flash_partition_t partition);
void consume_data(flash_partition_t partition);
void write_data(flash_partition_t partition);

#endif /* _STORAGE_HELPER_H_ */