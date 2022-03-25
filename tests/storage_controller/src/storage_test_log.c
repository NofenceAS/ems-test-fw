/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "storage.h"
#include "storage_helper.h"
#include "log_structure.h"

#include "pm_config.h"
#include <stdlib.h>

log_rec_t dummy_log = { .seq_1.has_usBatteryVoltage = true,
			.seq_1.usBatteryVoltage = 3300,

			.seq_2.has_bme280 = true,
			.seq_2.bme280.ulHumidity = 40,
			.seq_2.bme280.ulPressure = 105,
			.seq_2.bme280.ulTemperature = 25 };

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
	zassert_equal(stg_write_log_data((uint8_t *)&dummy_log, dummy_log_len),
		      0, "Write log error.");
	zassert_equal(stg_read_log_data(read_callback_log, 0), 0,
		      "Read log error.");
}

void test_log_extended(void)
{
	for (int i = 0; i < expected_log_entries; i++) {
		zassert_equal(stg_write_log_data((uint8_t *)&dummy_log,
						 dummy_log_len),
			      0, "Write log error.");
	}
	zassert_equal(stg_read_log_data(read_callback_multiple_log, 0), 0,
		      "Read log error.");
	zassert_equal(num_multiple_log_reads, expected_log_entries, "");
}

void test_reboot_persistent_log(void)
{
	zassert_equal(stg_write_log_data((uint8_t *)&dummy_log, dummy_log_len),
		      0, "Write log error.");

	/* Reset FCB, pretending to reboot. Checks if persistent storage
	 * works. Should get the expected read value from the written one above.
	 */
	int err = stg_fcb_reset_and_init();
	zassert_equal(err, 0, "Error simulating reboot and FCB resets.");

	zassert_equal(stg_read_log_data(read_callback_log, 0), 0,
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
	zassert_equal(stg_read_log_data(read_callback_log, 0), -ENODATA,
		      "Read log should return -ENODATA.");
}

/** @brief Test to see nothing happens and we get no error if we try to clear
 *         an empty fcb.
 */
void test_double_clear(void)
{
	/* Clear partition. */
	zassert_equal(stg_clear_partition(STG_PARTITION_LOG), 0, "");
	zassert_equal(stg_clear_partition(STG_PARTITION_LOG), 0, "");
}

void test_rotate_handling(void)
{
	/* Write entries greater than partition size. */
	uint32_t num_entries = (PM_LOG_PARTITION_SIZE / sizeof(log_rec_t)) + 1;
	for (int i = 0; i < num_entries; i++) {
		zassert_equal(stg_write_log_data((uint8_t *)&dummy_log,
						 dummy_log_len),
			      0, "Write log error.");
	}

	uint32_t expected_entries = get_num_entries(STG_PARTITION_LOG);
	num_multiple_log_reads = 0;

	zassert_equal(stg_read_log_data(read_callback_multiple_log, 0), 0, "");
	zassert_equal(num_multiple_log_reads, expected_entries, "");

	/* Write 50 times since we know we have to rotate once. */
	for (int i = 0; i < 50; i++) {
		zassert_equal(stg_write_log_data((uint8_t *)&dummy_log,
						 dummy_log_len),
			      0, "Write log error.");
	}

	expected_entries = 50;
	num_multiple_log_reads = 0;

	zassert_equal(stg_read_log_data(read_callback_multiple_log, 0), 0, "");
	zassert_equal(num_multiple_log_reads, expected_entries, "");
}

/** @brief Checks if it remembers the entries being read before.
 */
void test_log_after_reboot(void)
{
	/* Clear partition. */
	zassert_equal(stg_clear_partition(STG_PARTITION_LOG), 0, "");

	/* Write 10 entries. */
	for (int i = 0; i < 10; i++) {
		zassert_equal(stg_write_log_data((uint8_t *)&dummy_log,
						 dummy_log_len),
			      0, "Write log error.");
	}

	/* Read and expect 4 entries. */
	num_multiple_log_reads = 0;
	zassert_equal(stg_read_log_data(read_callback_multiple_log, 4), 0, "");
	zassert_equal(num_multiple_log_reads, 4, "");

	/* Simulate reboot. */
	int err = stg_fcb_reset_and_init();
	zassert_equal(err, 0, "Error simulating reboot and FCB resets.");

	/* We know the pointer is not persistent, so we still expect 10 entries. */
	num_multiple_log_reads = 0;
	zassert_equal(stg_read_log_data(read_callback_multiple_log, 0), 0, "");
	zassert_equal(num_multiple_log_reads, 10, "");

	/* We're done reading, erase contents since we're done with it. */
	zassert_equal(stg_clear_partition(STG_PARTITION_LOG), 0, "");

	/* Simulate reboot. */
	err = stg_fcb_reset_and_init();
	zassert_equal(err, 0, "Error simulating reboot and FCB resets.");

	/* We should now have 0 entries remaining due to clear fcb
	 * partition. Should return -ENODATA. 
	 */
	zassert_equal(stg_read_log_data(read_callback_multiple_log, 0),
		      -ENODATA, "");
}