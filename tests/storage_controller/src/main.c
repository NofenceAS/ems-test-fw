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

K_SEM_DEFINE(write_log_ack_sem, 0, 1);
K_SEM_DEFINE(read_log_ack_sem, 0, 1);
K_SEM_DEFINE(consumed_log_ack_sem, 0, 1);

K_SEM_DEFINE(write_ano_ack_sem, 0, 1);
K_SEM_DEFINE(read_ano_ack_sem, 0, 1);
K_SEM_DEFINE(consumed_ano_ack_sem, 0, 1);

K_SEM_DEFINE(write_pasture_ack_sem, 0, 1);
K_SEM_DEFINE(read_pasture_ack_sem, 0, 1);
K_SEM_DEFINE(consumed_pasture_ack_sem, 0, 1);

/* Used to check which partition was used, to release the correct semaphore. */
flash_partition_t cur_partition;

test_id_t cur_test_id = TEST_ID_NORMAL;

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

void test_clear_fcbs(void)
{
	int err;

	zassert_equal(clear_fcb_sectors(STG_PARTITION_LOG), 0, "");
	zassert_equal(clear_fcb_sectors(STG_PARTITION_ANO), 0, "");
	zassert_equal(clear_fcb_sectors(STG_PARTITION_PASTURE), 0, "");

	k_sleep(K_SECONDS(30));

	/* Read and see that the semaphores lock, since we do not get
	 * callbacks due to empty FCBs.
	 */
	request_log_data();
	err = k_sem_take(&read_log_ack_sem, K_SECONDS(30));
	zassert_not_equal(err, 0, "");
	err = k_sem_take(&consumed_log_ack_sem, K_SECONDS(30));
	zassert_not_equal(err, 0, "");

	request_ano_data();
	err = k_sem_take(&read_log_ack_sem, K_SECONDS(30));
	zassert_not_equal(err, 0, "");
	err = k_sem_take(&consumed_log_ack_sem, K_SECONDS(30));
	zassert_not_equal(err, 0, "");

	request_pasture_data();
	err = k_sem_take(&read_pasture_ack_sem, K_SECONDS(30));
	zassert_not_equal(err, 0, "");
	err = k_sem_take(&consumed_pasture_ack_sem, K_SECONDS(30));
	zassert_not_equal(err, 0, "");
}

/** @brief Tests what happens when we mix up the request/write calls to
 *         the storage controller, which is bound to happen in the system
 *         since multiple modules operate simultaneously and will 
 *         both request and write at the same time.
 */
void test_pasture_log_ano_write_read(void)
{
	/* Mix up the function calls, do not wait for semaphores.
	 * You cannot submit the same event subsequently, especially 
	 * not from the same thread since that would
	 * make the event_handler thread not process any of the events
	 * but the last. So duplicate calls is not performed in this test. 
	 */
	write_pasture_data(13);
	write_log_data();
	request_log_data();
	write_ano_data();
	request_ano_data();
	request_pasture_data();

	k_sleep(K_SECONDS(30));

	/* Test finished when we've consumed all the data, these are
	 * for log, ano and pasture data. However, we only need to check
	 * if we've consumed the pasture data, since that is the last
	 * function call. (Keep in mind that we also check the contents
	 * of every other function call in storage_helper.c)
	 * 
	 * Since the storage controller runs on a separate thread and uses
	 * message queue, we expect it to process all of the above, 
	 * regardless of order.
	 */
	int err = k_sem_take(&consumed_pasture_ack_sem, K_SECONDS(30));
	zassert_equal(
		err, 0,
		"Test execution hanged waiting for consumed pasture ack.");

	k_sleep(K_SECONDS(30));
}

void setup_common(void)
{
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

void teardown_common(void)
{
	/* Ensure other threads in use have time to finish what they're doing.
	 * This does makes no difference to threads that are coopartive (<-1)
	 */
	k_sleep(K_SECONDS(30));
}

void test_main(void)
{
	/* Init FCBs and event manager setup. */
	ztest_test_suite(storage_init, ztest_unit_test(test_event_manager_init),
			 ztest_unit_test(test_init));
	ztest_run_test_suite(storage_init);

	/* Test log partition and read/write/consumes. */
	ztest_test_suite(
		storage_log_test,
		ztest_unit_test_setup_teardown(test_log_write, setup_common,
					       teardown_common),
		ztest_unit_test_setup_teardown(test_log_read, setup_common,
					       teardown_common),
		ztest_unit_test_setup_teardown(test_write_log_exceed,
					       setup_common, teardown_common),
		ztest_unit_test_setup_teardown(test_read_log_exceed,
					       setup_common, teardown_common),
		ztest_unit_test_setup_teardown(test_empty_walk_log,
					       setup_common, teardown_common),
		ztest_unit_test_setup_teardown(test_reboot_persistent_log,
					       setup_common, teardown_common));
	ztest_run_test_suite(storage_log_test);

	/* Test ano partition and read/write/consumes. */
	ztest_test_suite(
		storage_ano_test,
		ztest_unit_test_setup_teardown(test_ano_write, setup_common,
					       teardown_common),
		ztest_unit_test_setup_teardown(test_ano_read, setup_common,
					       teardown_common),
		ztest_unit_test_setup_teardown(test_reboot_persistent_ano,
					       setup_common, teardown_common));
	ztest_run_test_suite(storage_ano_test);

	/* Test pasture partition and read/write/consumes. */
	ztest_test_suite(
		storage_pasture_test,
		ztest_unit_test_setup_teardown(test_pasture_write, setup_common,
					       teardown_common),
		ztest_unit_test_setup_teardown(test_pasture_read, setup_common,
					       teardown_common),
		ztest_unit_test_setup_teardown(test_reboot_persistent_pasture,
					       setup_common, teardown_common),
		ztest_unit_test_setup_teardown(test_pasture_extended_write_read,
					       setup_common, teardown_common),
		ztest_unit_test_setup_teardown(test_request_pasture_twice,
					       setup_common, teardown_common));
	ztest_run_test_suite(storage_pasture_test);

	/* Integration testing of all partitions and multiple read/write/consume
	 * across all partitions and all FCBs. 
	 */
	ztest_test_suite(
		storage_integration,
		ztest_unit_test_setup_teardown(test_clear_fcbs, setup_common,
					       teardown_common),
		ztest_unit_test_setup_teardown(test_pasture_log_ano_write_read,
					       setup_common, teardown_common));
	ztest_run_test_suite(storage_integration);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_stg_ack_write_event(eh)) {
		struct stg_ack_write_event *ev = cast_stg_ack_write_event(eh);
		if (ev->partition == STG_PARTITION_LOG) {
			k_sem_give(&write_log_ack_sem);

			if (write_log_ptr != NULL) {
				k_free(write_log_ptr);
				write_log_ptr = NULL;
			}
			/* Add a new entry in our counter, however,
			 * if the write event rotated since it was full,
			 * we have to decrease the counter based on how many
			 * entries fit on a single sector.
			 */
			num_new_entries++;
			if (ev->rotated) {
				int removed_entries =
					(int)(SECTOR_SIZE / sizeof(log_rec_t));
				num_new_entries -= removed_entries;
			}
		} else if (ev->partition == STG_PARTITION_ANO) {
			k_sem_give(&write_ano_ack_sem);

			if (write_ano_ptr != NULL) {
				k_free(write_ano_ptr);
				write_ano_ptr = NULL;
			}

		} else if (ev->partition == STG_PARTITION_PASTURE) {
			k_sem_give(&write_pasture_ack_sem);

			if (write_pasture_ptr != NULL) {
				k_free(write_pasture_ptr);
				write_pasture_ptr = NULL;
			}
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
			consume_data(ev);
		} else if (ev->partition == STG_PARTITION_ANO) {
			k_sem_give(&read_ano_ack_sem);
			consume_data(ev);
		} else if (ev->partition == STG_PARTITION_PASTURE) {
			k_sem_give(&read_pasture_ack_sem);
			/* Consume the data, where we also publish consumed events
			 * once we're done with the data. This continues the walk
			 * process and can trigger further read acks.
			 */
			consume_data(ev);
		} else {
			zassert_unreachable(
				"Unexpected ack write partition recieved.");
		}

		/* After contents have been consumed successfully, 
		 * notify the storage controller to continue walk if there
		 * are any new entries.
		 */
		struct stg_consumed_read_event *ev_read =
			new_stg_consumed_read_event();
		EVENT_SUBMIT(ev_read);

		/* Sleep thread so storage controller can catch the consume
		 * event and process further reads.
		 */
		k_sleep(K_SECONDS(1));
		return false;
	}
	if (is_stg_consumed_read_event(eh)) {
		if (cur_partition == STG_PARTITION_LOG) {
			k_sem_give(&consumed_log_ack_sem);
		} else if (cur_partition == STG_PARTITION_ANO) {
			k_sem_give(&consumed_ano_ack_sem);
		} else if (cur_partition == STG_PARTITION_PASTURE) {
			k_sem_give(&consumed_pasture_ack_sem);
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
