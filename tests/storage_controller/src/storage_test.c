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

static K_SEM_DEFINE(write_log_ack_sem, 0, 1);
static K_SEM_DEFINE(read_log_ack_sem, 0, 1);
static K_SEM_DEFINE(consumed_log_ack_sem, 0, 1);

static K_SEM_DEFINE(write_ano_ack_sem, 0, 1);
static K_SEM_DEFINE(read_ano_ack_sem, 0, 1);
static K_SEM_DEFINE(consumed_ano_ack_sem, 0, 1);

int consumed_entries_counter = 0;

/* Provide custom assert post action handler to handle the assertion on OOM
 * error in Event Manager.
 */
BUILD_ASSERT(!IS_ENABLED(CONFIG_ASSERT_NO_FILE_INFO));
void assert_post_action(const char *file, unsigned int line)
{
	printk("assert_post_action - file: %s (line: %u)\n", file, line);
}

void test_event_manager_init(void)
{
	zassert_false(event_manager_init(),
		      "Error when initializing event manager");
}

void test_init(void)
{
	zassert_false(init_storage_controller(),
		      "Error when initializing storage controller.");

	k_sleep(K_SECONDS(30));
}

void test_append_log_data(void)
{
	write_data(STG_PARTITION_LOG);
	int err = k_sem_take(&write_log_ack_sem, K_SECONDS(30));
	zassert_equal(err, 0,
		      "Test execution hanged waiting for write log ack.");
}

void test_empty_walk(void)
{
	/* Expect semaphore to hang, since we do not get any callbacks. */
	request_data(STG_PARTITION_LOG);

	int err = k_sem_take(&read_log_ack_sem, K_SECONDS(30));
	zassert_not_equal(
		err, 0, "Test execution should hang waiting for read log ack.");

	/* We process the data further in event_handler. */

	err = k_sem_take(&consumed_log_ack_sem, K_SECONDS(30));
	zassert_not_equal(
		err, 0,
		"Test execution should hang waiting for consumed log ack.");
}

void test_read_log_data(void)
{
	/* First we request, then we get callback in event_handler when
	 * data is ready. There might be multiple entries since
	 * last read, so the event_handler can trigger several times
	 * with new entries. We just have to take care of the
	 * consume_event once we've consumed the data.
	 */
	request_data(STG_PARTITION_LOG);

	int err = k_sem_take(&read_log_ack_sem, K_SECONDS(30));
	zassert_equal(err, 0,
		      "Test execution hanged waiting for read log ack.");

	/* We process the data further in event_handler. */

	err = k_sem_take(&consumed_log_ack_sem, K_SECONDS(30));
	zassert_equal(err, 0,
		      "Test execution hanged waiting for consumed log ack.");

	/* We know at this stage that we do not have more entries, test done. */
}

void setup_multiple_reads(void)
{
	consumed_entries_counter = 0;
}

void test_multiple_writes_and_verify(void)
{
	/* Writes 14 log sectors, and 8 ano 
	 * sectors based on DEFINES in storage_helper.h. 
	 */
	int log_writes = 14;
	int ano_writes = 8;

	int err;

	/* LOG entires. */
	for (int i = 0; i < log_writes; i++) {
		write_data(STG_PARTITION_LOG);

		err = k_sem_take(&write_log_ack_sem, K_SECONDS(30));
		zassert_equal(
			err, 0,
			"Test execution hanged waiting for write log ack.");
	}

	/* ANO entires. */
	for (int i = 0; i < ano_writes; i++) {
		write_data(STG_PARTITION_ANO);

		err = k_sem_take(&write_ano_ack_sem, K_SECONDS(30));
		zassert_equal(
			err, 0,
			"Test execution hanged waiting for write ano ack.");
	}

	consumed_entries_counter = 0;
	/* Walk LOG data entries. */
	request_data(STG_PARTITION_LOG);

	/* Wait until the consume_data function triggered in event_handler
	 * is finished with all the entries. We know how many
	 * consumed_event acks we can expect.
	 */
	for (int i = 0; i < log_writes; i++) {
		err = k_sem_take(&consumed_log_ack_sem, K_SECONDS(30));
		zassert_equal(
			err, 0,
			"Test hanged waiting for multiple consumed log ack.");
	}
	/* Wait for event handler thread to process last consume_data. */
	k_sleep(K_SECONDS(60));

	/* Check if the consumed logic was processed 
	 * the correct number of times. The content is checked with zasserts
	 * within the consume_data function itself.
	 */
	zassert_equal(consumed_entries_counter, log_writes,
		      "Mismatched consumed log entries.");

	/* Perpare for walk ANO data entries. */
	consumed_entries_counter = 0;

	request_data(STG_PARTITION_ANO);
	for (int i = 0; i < ano_writes; i++) {
		err = k_sem_take(&consumed_ano_ack_sem, K_SECONDS(30));
		zassert_equal(
			err, 0,
			"Test hanged waiting for multiple consumed ano ack.");
	}
	/* Wait for event handler thread to process last consume_data. */
	k_sleep(K_SECONDS(60));
	zassert_equal(consumed_entries_counter, ano_writes,
		      "Mismatched consumed ano entries.");
}

void test_append_too_many(void)
{
	/* Used as ID written on each sector. */
	consumed_entries_counter = 0;

	/* It's difficult to calculate precicely how many reads
	 * will be performed, due to padding within the FLASH_AREA partition.
	 * Assume we have mem_rec structure, which consists of 264 bytes and we
	 * have 0x20000 (128kb) memory, we can estimate 496.484848 - 1 reads,
	 * however, FCB adds padding inbetween entries as well as header,
	 * since the correct number of reads with mem_rec seems to be around 
	 * 470.
	 */
	int num_log_sectors = 1024;
	int num_ano_sectors = 256;

	int err;

	for (int i = 0; i < num_log_sectors; i++) {
		write_data(STG_PARTITION_LOG);
		err = k_sem_take(&write_log_ack_sem, K_SECONDS(30));
		zassert_equal(
			err, 0,
			"Test execution hanged waiting for write log ack.");
	}

	/* Used as ID written on each sector. */
	consumed_entries_counter = 0;

	for (int i = 0; i < num_ano_sectors; i++) {
		write_data(STG_PARTITION_ANO);
		err = k_sem_take(&write_ano_ack_sem, K_SECONDS(30));
		zassert_equal(
			err, 0,
			"Test execution hanged waiting for write ano ack.");
	}
}

/** @brief Checks what happens if we try to read after there's a long time
*          since we've read previously.
*/
void test_read_too_many(void)
{
	/* It's difficult to calculate precicely how many reads
	 * will be performed, due to padding within the FLASH_AREA partition.
	 * Assume we have mem_rec structure, which consists of 264 bytes and we
	 * have 0x20000 (128kb) memory, we can estimate 496.484848 - 1 reads,
	 * however, FCB adds padding inbetween entries as well as header,
	 * since the correct number of reads with mem_rec seems to be around 
	 * 470.
	 */
	int log_writes = 1024;
	int ano_writes = 256;

	int err;

	consumed_entries_counter = 0;
	/* Walk LOG data entries. */
	request_data(STG_PARTITION_LOG);

	/* Wait until the consume_data function triggered in event_handler
	 * is finished with all the entries. We know how many
	 * consumed_event acks we can expect.
	 */
	for (int i = 0; i < log_writes; i++) {
		err = k_sem_take(&consumed_log_ack_sem, K_SECONDS(30));
		zassert_equal(
			err, 0,
			"Test hanged waiting for multiple consumed log ack.");
	}
	/* Wait for event handler thread to process last consume_data. */
	k_sleep(K_SECONDS(60));

	/* Check if the consumed logic was processed 
	 * the correct number of times. The content is checked with zasserts
	 * within the consume_data function itself.
	 */
	zassert_equal(consumed_entries_counter, log_writes,
		      "Mismatched consumed log entries.");

	/* Perpare for walk ANO data entries. */
	consumed_entries_counter = 0;

	request_data(STG_PARTITION_ANO);
	for (int i = 0; i < ano_writes; i++) {
		err = k_sem_take(&consumed_ano_ack_sem, K_SECONDS(30));
		zassert_equal(
			err, 0,
			"Test hanged waiting for multiple consumed ano ack.");
	}
	/* Wait for event handler thread to process last consume_data. */
	k_sleep(K_SECONDS(60));
	zassert_equal(consumed_entries_counter, ano_writes,
		      "Mismatched consumed ano entries.");
}

void test_clear_fcbs(void)
{
	int err;

	zassert_equal(clear_fcb_sectors(STG_PARTITION_LOG), 0, "");
	zassert_equal(clear_fcb_sectors(STG_PARTITION_ANO), 0, "");

	k_sleep(K_SECONDS(60));

	/* Read and see that the semaphores lock, since we do not get
	 * callbacks due to empty FCBs.
	 */
	request_data(STG_PARTITION_LOG);
	err = k_sem_take(&read_log_ack_sem, K_SECONDS(30));
	zassert_not_equal(err, 0, "");
	err = k_sem_take(&consumed_log_ack_sem, K_SECONDS(30));
	zassert_not_equal(err, 0, "");

	request_data(STG_PARTITION_ANO);
	err = k_sem_take(&read_log_ack_sem, K_SECONDS(30));
	zassert_not_equal(err, 0, "");
	err = k_sem_take(&consumed_log_ack_sem, K_SECONDS(30));
	zassert_not_equal(err, 0, "");
}

void setup_take_semaphores_reset_counter(void)
{
	/* Used when counting entries in walks. */
	consumed_entries_counter = 0;

	/* Take semaphores to ensure they are 
	 * taken prior to the tests that need them. 
	 */
	k_sem_take(&read_log_ack_sem, K_SECONDS(30));
	k_sem_take(&read_ano_ack_sem, K_SECONDS(30));

	k_sem_take(&consumed_log_ack_sem, K_SECONDS(30));
	k_sem_take(&consumed_ano_ack_sem, K_SECONDS(30));

	k_sem_take(&write_log_ack_sem, K_SECONDS(30));
	k_sem_take(&write_ano_ack_sem, K_SECONDS(30));
}

void teardown_tidy_threads(void)
{
	/* Ensure other threads in use have time to finish what they're doing.
	 * This does makes no difference to threads that are coopartive (<-1)
	 */
	k_sleep(K_SECONDS(60));
}

void test_main(void)
{
	ztest_test_suite(
		storage_controller_tests,
		ztest_unit_test(test_event_manager_init),
		ztest_unit_test(test_init),
		ztest_unit_test_setup_teardown(
			test_empty_walk, setup_take_semaphores_reset_counter,
			teardown_tidy_threads),
		ztest_unit_test_setup_teardown(
			test_append_log_data,
			setup_take_semaphores_reset_counter,
			teardown_tidy_threads),
		ztest_unit_test_setup_teardown(
			test_read_log_data, setup_take_semaphores_reset_counter,
			teardown_tidy_threads),
		ztest_unit_test_setup_teardown(
			test_multiple_writes_and_verify,
			setup_take_semaphores_reset_counter,
			teardown_tidy_threads),
		ztest_unit_test_setup_teardown(
			test_append_too_many,
			setup_take_semaphores_reset_counter,
			teardown_tidy_threads),
		ztest_unit_test_setup_teardown(
			test_read_too_many, setup_take_semaphores_reset_counter,
			teardown_tidy_threads),
		ztest_unit_test_setup_teardown(
			test_clear_fcbs, setup_take_semaphores_reset_counter,
			teardown_tidy_threads));
	ztest_run_test_suite(storage_controller_tests);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_stg_ack_write_event(eh)) {
		struct stg_ack_write_event *ev = cast_stg_ack_write_event(eh);

		if (ev->partition == STG_PARTITION_LOG) {
			k_sem_give(&write_log_ack_sem);
		} else if (ev->partition == STG_PARTITION_ANO) {
			k_sem_give(&write_ano_ack_sem);
		} else {
			zassert_unreachable(
				"Unexpected ack write partition recieved.");
		}
		return false;
	}
	if (is_stg_ack_read_event(eh)) {
		struct stg_ack_read_event *ev = cast_stg_ack_read_event(eh);

		if (ev->partition == STG_PARTITION_LOG) {
			k_sem_give(&read_log_ack_sem);
		} else if (ev->partition == STG_PARTITION_ANO) {
			k_sem_give(&read_ano_ack_sem);
		} else {
			zassert_unreachable(
				"Unexpected ack read partition recieved.");
		}
		/* Consume the data, where we also publish consumed events
		 * once we're done with the data. This continues the walk
		 * process and can trigger further read acks.
		 */
		consume_data(ev->partition);
		return false;
	}
	if (is_stg_consumed_read_event(eh)) {
		struct stg_consumed_read_event *ev =
			cast_stg_consumed_read_event(eh);

		if (ev->partition == STG_PARTITION_LOG) {
			k_sem_give(&consumed_log_ack_sem);
		} else if (ev->partition == STG_PARTITION_ANO) {
			k_sem_give(&consumed_ano_ack_sem);
		} else {
			zassert_unreachable(
				"Unexpected ack read partition recieved.");
		}
		return false;
	}
	return false;
}

EVENT_LISTENER(test_main, event_handler);

EVENT_SUBSCRIBE(test_main, stg_ack_write_event);
EVENT_SUBSCRIBE(test_main, stg_ack_read_event);
EVENT_SUBSCRIBE(test_main, stg_consumed_read_event);
