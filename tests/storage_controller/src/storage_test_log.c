/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "storage.h"
#include "storage_helper.h"
#include "log_structure.h"

#include "pm_config.h"
#include <stdlib.h>

log_rec_t dummy_log = { .header.len = 4, .buf = { 0xDE, 0xAD, 0xBE, 0xEF } };
size_t dummy_log_len = sizeof(log_rec_t);

int read_callback_log(uint8_t *data, size_t len)
{
	zassert_equal(len, dummy_log_len, "");
	zassert_mem_equal((log_rec_t *)data, &dummy_log, len, "");
	return 0;
}

static int num_multiple_log_reads = 0;
static int expected_log_entries = 5;

int read_callback_multiple_log(uint8_t *data, size_t len)
{
	num_multiple_log_reads++;
	return read_callback_log(data, len);
}

void test_log(void)
{
	zassert_equal(stg_write_to_partition(STG_PARTITION_LOG,
					     (uint8_t *)&dummy_log,
					     dummy_log_len),
		      0, "Write log error.");
	zassert_equal(stg_read_log_data(read_callback_log), 0,
		      "Read log error.");
}

void test_log_extended(void)
{
	for (int i = 0; i < expected_log_entries; i++) {
		zassert_equal(stg_write_to_partition(STG_PARTITION_LOG,
						     (uint8_t *)&dummy_log,
						     dummy_log_len),
			      0, "Write log error.");
	}
	zassert_equal(stg_read_log_data(read_callback_multiple_log), 0,
		      "Read log error.");
	zassert_equal(num_multiple_log_reads, expected_log_entries, "");
}

void test_reboot_persistent_log(void)
{
	zassert_equal(stg_write_to_partition(STG_PARTITION_LOG,
					     (uint8_t *)&dummy_log,
					     dummy_log_len),
		      0, "Write log error.");

	/* Reset FCB, pretending to reboot. Checks if persistent storage
	 * works. Should get the expected read value from the written one above.
	 */
	int err = stg_fcb_reset_and_init();
	zassert_equal(err, 0, "Error simulating reboot and FCB resets.");

	zassert_equal(stg_read_log_data(read_callback_log), 0,
		      "Read log error.");
}

/** @brief Test to check that we get ENODATA if there are no data available
 *         on the partition.
 */
void test_no_log_available(void)
{
	/* Clear partition. */
	zassert_equal(stg_clear_partition(STG_PARTITION_LOG), 0, "");

	/* Read. */
	zassert_equal(stg_read_log_data(read_callback_log), -ENODATA,
		      "Read log should return -ENODATA.");
}