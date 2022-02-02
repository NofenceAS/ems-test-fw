/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "storage.h"
#include "storage_events.h"
#include "storage_helper.h"

#include "ano_structure.h"
#include "log_structure.h"
#include "pasture_structure.h"

#include "pm_config.h"
#include <stdlib.h>

int num_new_entries = 0;
bool rotated_log = false;

/* Number of entries available from the partition we have emulated if no 
 * headers/padding is added, which it is. We use this number to ensure
 * we exceed the partition size when we test to append too many entries.
 */
#define LOG_ENTRY_COUNT PM_LOG_PARTITION_SIZE / sizeof(log_rec_t)

void test_append_log_data(void)
{
	write_log_data();
	int err = k_sem_take(&write_log_ack_sem, K_SECONDS(30));
	zassert_equal(err, 0,
		      "Test execution hanged waiting for write log ack.");
}

void test_read_log_data(void)
{
	/* First we request, then we get callback in event_handler when
	 * data is ready. There might be multiple entries since
	 * last read, so the event_handler can trigger several times
	 * with new entries. We just have to take care of the
	 * consume_event once we've consumed the data.
	 */
	request_log_data();
	int err = k_sem_take(&read_log_ack_sem, K_SECONDS(30));
	zassert_equal(err, 0,
		      "Test execution hanged waiting for read log ack.");

	/* We process the data further in event_handler. */

	err = k_sem_take(&consumed_log_ack_sem, K_SECONDS(30));
	zassert_equal(err, 0,
		      "Test execution hanged waiting for consumed log ack.");
}

void test_append_too_many_log(void)
{
	/* We can calculate the minimum amount of entries needed
	 * to fill up the sectors, so add 1 to exceed the limit and
	 * expect the storage controller to rotate FCB since it returns
	 * -ENOSPC.
	 */
	int num_log_sectors = LOG_ENTRY_COUNT + 1;

	int err;

	for (int i = 0; i < num_log_sectors; i++) {
		write_log_data();
		err = k_sem_take(&write_log_ack_sem, K_SECONDS(30));
		zassert_equal(
			err, 0,
			"Test execution hanged waiting for write log ack.");
		k_sleep(K_MSEC(50));
	}
}

/** @brief Checks what happens if we try to read after there's a long time
*          since we've read previously.
*/
void test_read_too_many_log(void)
{
	cur_test_id = TEST_ID_MASS_LOG_WRITES;
	first_log_read = true;

	int log_writes = num_new_entries - 1;

	printk("There are %i new entries available.\n", log_writes);

	int err;
	/* Walk LOG data entries. */
	request_log_data();

	/* Wait until the consume_data function triggered in event_handler
	 * is finished with all the entries. We know how many
	 * consumed_event acks we can expect.
	 */
	for (int i = 0; i < log_writes; i++) {
		err = k_sem_take(&consumed_log_ack_sem, K_SECONDS(30));
		zassert_equal(
			err, 0,
			"Test hanged waiting for multiple consumed log ack.");
		k_sleep(K_MSEC(100));
	}
	num_new_entries = 0;

	cur_test_id = TEST_ID_NORMAL;

	k_sleep(K_SECONDS(30));
}