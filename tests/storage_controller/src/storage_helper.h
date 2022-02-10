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

/** Variables used to check how many times we had to rotate FCB during write
 *  By this, we can estimate how many read operations (walk callbacks) will
 *  be triggered and put the numbers into zassert. We only have this for LOG
 *  since ANO and PASTURE is never full as we only care about the newest entry.
 * 
 *  Basically used by calculating how many entries are added when it's full.
 * 
 *  first_log_read is also included, which simply reads the value at flash
 *  so that we can expect all of the next reads to be +1.
 *  We need this because when we start to rotate/replace old entries before
 *  we're able to read them, we cannot verify the contents
 *  since we do not know the relation between write and read pointer on the FCB.
 */
extern int num_new_entries;
extern bool first_log_read;

extern int log_write_index_value;
extern int log_read_index_value;

extern int ano_write_index_value;
extern int ano_read_index_value;

/* Pasture tests. */
void test_pasture_write(void);
void test_pasture_read(void);
void test_pasture_extended_write_read(void);
void test_reboot_persistent_pasture(void);
void test_request_pasture_twice(void);

/* Log tests. */
void test_log_write(void);
void test_log_read(void);
void test_reboot_persistent_log(void);
void test_write_log_exceed(void);
void test_read_log_exceed(void);
void test_empty_walk_log(void);

/* Ano tests. */
void test_ano_write(void);
void test_ano_read(void);
void test_reboot_persistent_ano(void);

typedef enum {
	TEST_ID_NORMAL = 0,
	TEST_ID_MASS_LOG_WRITES = 1,
	TEST_ID_PASTURE_DYNAMIC = 2
} test_id_t;
extern test_id_t cur_test_id;

#endif /* _STORAGE_HELPER_H_ */