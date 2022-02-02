/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _STORAGE_HELPER_H_
#define _STORAGE_HELPER_H_

#include <ztest.h>
#include "storage.h"
#include "storage_events.h"

void request_data(flash_partition_t partition);
void write_data(flash_partition_t partition);

/** @brief Function that consumes data acquired from the ack read event.
 *         The ack read has a uint8_t* data pointer, as well as
 *         a size_t len field, which can be used to typecast to struct necessary
 *         based on partition.
 */
void consume_data(struct stg_ack_read_event *event);

extern int consumed_entries_counter;
extern uint8_t *write_ptr;
extern flash_partition_t cur_partition;

void test_pasture_write(void);
void test_pasture_read(void);
void test_pasture_extended_write_read(void);

#endif /* _STORAGE_HELPER_H_ */