/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _STORAGE_HELPER_H_
#define _STORAGE_HELPER_H_

#include <ztest.h>
#include "storage.h"
#include "storage_events.h"

void request_log_data();
void request_ano_data();
void request_pasture_data();

void write_log_data();
void write_ano_data();
void write_pasture_data(uint8_t num_points);

/** @brief Function that consumes data acquired from the ack read event.
 *         The ack read has a uint8_t* data pointer, as well as
 *         a size_t len field, which can be used to typecast to struct necessary
 *         based on partition.
 */
void consume_data(struct stg_ack_read_event *event);

extern int consumed_entries_counter;

extern uint8_t *write_log_ptr;
extern uint8_t *write_ano_ptr;
extern uint8_t *write_pasture_ptr;

extern flash_partition_t cur_partition;

extern struct k_sem write_log_ack_sem;
extern struct k_sem read_log_ack_sem;
extern struct k_sem consumed_log_ack_sem;

extern struct k_sem write_ano_ack_sem;
extern struct k_sem read_ano_ack_sem;
extern struct k_sem consumed_ano_ack_sem;

extern struct k_sem write_pasture_ack_sem;
extern struct k_sem read_pasture_ack_sem;
extern struct k_sem consumed_pasture_ack_sem;

void test_pasture_write(void);
void test_pasture_read(void);
void test_pasture_log_ano_write_read(void);
void test_pasture_extended_write_read(void);

typedef enum { TEST_ID_INIT = 0, TEST_ID_DYNAMIC = 1 } test_id_t;
extern test_id_t cur_test_id;

#endif /* _STORAGE_HELPER_H_ */