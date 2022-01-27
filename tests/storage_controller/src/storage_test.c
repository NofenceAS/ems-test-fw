/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "storage.h"
#include "storage_events.h"
#include "flash_memory.h"
#include "storage_helper.h"

static K_SEM_DEFINE(write_log_ack_sem, 0, 1);
static K_SEM_DEFINE(read_log_ack_sem, 0, 1);
static K_SEM_DEFINE(consumed_log_ack_sem, 0, 1);

static K_SEM_DEFINE(write_ano_ack_sem, 0, 1);
static K_SEM_DEFINE(read_ano_ack_sem, 0, 1);
static K_SEM_DEFINE(consumed_ano_ack_sem, 0, 1);

mem_rec cached_mem_rec;

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

void test_storage_init(void)
{
	zassert_false(init_storage_controller(),
		      "Error when initializing storage controller.");

	k_sleep(K_SECONDS(30));
}

void test_storage_append_log_data(void)
{
	write_data(STG_PARTITION_LOG);
	int err = k_sem_take(&write_log_ack_sem, K_SECONDS(30));
	zassert_equal(err, 0,
		      "Test execution hanged waiting for write log ack.");
}

void test_storage_read_log_data(void)
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

/** @brief Writes 3 log messages, then 5 ano messages. Then we read to verify.
 */
//void test_multiple_writes_and_reads(void)
//{
//	int log_writes = 3;
//	int ano_writes = 5;
//	mem_rec dummy_data;
//
//	for (int i = 0; i < log_writes; i++) {
//		memset(&dummy_memrec, 0, sizeof(mem_rec));
//		dummy_memrec.header.ID = 1337;
//
//		struct stg_write_memrec_event *ev =
//			new_stg_write_memrec_event();
//		ev->new_rec = &dummy_memrec;
//		ev->partition = STG_PARTITION_LOG;
//		EVENT_SUBMIT(ev);
//
//		int err = k_sem_take(&write_log_ack_sem, K_SECONDS(30));
//		zassert_equal(
//			err, 0,
//			"Test execution hanged waiting for write log ack.");
//	}
//
//	for (int i = 0; i < ano_writes; i++) {
//		memset(&dummy_memrec, 0, sizeof(mem_rec));
//		dummy_memrec.header.ID = 1337;
//
//		struct stg_write_memrec_event *ev =
//			new_stg_write_memrec_event();
//		ev->new_rec = &dummy_memrec;
//		ev->partition = STG_PARTITION_ANO;
//		EVENT_SUBMIT(ev);
//
//		int err = k_sem_take(&write_ano_ack_sem, K_SECONDS(30));
//		zassert_equal(
//			err, 0,
//			"Test execution hanged waiting for write ano ack.");
//	}
//	/* Submit read request, will trigger walk function to walk over
//	 * all the new entries given. Stores entries left in
//	 * entries left variable.
//	 */
//	mem_rec out_data;
//
//	struct stg_read_memrec_event *ev = new_stg_read_memrec_event();
//	ev->new_rec = &out_data;
//	ev->partition = STG_PARTITION_LOG;
//	EVENT_SUBMIT(ev);
//	/* We only get this semaphore if data is read. Compare contents. */
//	int err = k_sem_take(&read_log_ack_sem, K_SECONDS(30));
//	zassert_equal(err, 0,
//		      "Test execution hanged waiting for read log ack.");
//
//	while (current_entries_left > 0) {
//		mem_rec expected;
//		memset(&expected, 2, sizeof(mem_rec));
//		zassert_mem_equal(&out_data, &expected, sizeof(mem_rec),
//				  "Memory contents are not equal.");
//
//		/* After contents have been consumed successfully,
//		 * notify the storage controller that we have consumed the data.
//		 */
//		struct stg_data_consumed_event *ev_consumed =
//			new_stg_data_consumed_event();
//		ev_consumed->partition = STG_PARTITION_LOG;
//		EVENT_SUBMIT(ev_consumed);
//	}
//}

void test_main(void)
{
	ztest_test_suite(storage_controller_tests,
			 ztest_unit_test(test_event_manager_init),
			 ztest_unit_test(test_storage_init),
			 ztest_unit_test(test_storage_append_log_data),
			 ztest_unit_test(test_storage_read_log_data));
	ztest_run_test_suite(storage_controller_tests);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_stg_ack_write_memrec_event(eh)) {
		struct stg_ack_write_memrec_event *ev =
			cast_stg_ack_write_memrec_event(eh);

		if (ev->partition == STG_PARTITION_LOG) {
			k_sem_give(&write_log_ack_sem);
		} else if (ev->partition == STG_PARTITION_ANO) {
			k_sem_give(&write_ano_ack_sem);
		} else {
			printk("is %i", ev->partition);
			zassert_unreachable(
				"Unexpected ack write partition recieved.");
		}
		return false;
	}
	if (is_stg_ack_read_memrec_event(eh)) {
		struct stg_ack_read_memrec_event *ev =
			cast_stg_ack_read_memrec_event(eh);

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
	if (is_stg_data_consumed_event(eh)) {
		struct stg_data_consumed_event *ev =
			cast_stg_data_consumed_event(eh);

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
EVENT_SUBSCRIBE(test_main, stg_write_memrec_event);
EVENT_SUBSCRIBE(test_main, stg_ack_write_memrec_event);
EVENT_SUBSCRIBE(test_main, stg_read_memrec_event);
EVENT_SUBSCRIBE(test_main, stg_ack_read_memrec_event);
EVENT_SUBSCRIBE(test_main, stg_data_consumed_event);
