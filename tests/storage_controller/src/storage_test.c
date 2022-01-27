/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "storage.h"
#include "storage_events.h"
#include "flash_memory.h"

static K_SEM_DEFINE(write_log_ack_sem, 0, 1);
static K_SEM_DEFINE(read_log_ack_sem, 0, 1);

static K_SEM_DEFINE(write_ano_ack_sem, 0, 1);
static K_SEM_DEFINE(read_ano_ack_sem, 0, 1);

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
}

mem_rec dummy_memrec;

void test_storage_append_log_data(void)
{
	memset(&dummy_memrec, 0, sizeof(mem_rec));

	struct stg_write_memrec_event *ev = new_stg_write_memrec_event();
	ev->new_rec = &dummy_memrec;
	ev->partition = STG_PARTITION_LOG;
	EVENT_SUBMIT(ev);

	int err = k_sem_take(&write_log_ack_sem, K_SECONDS(30));
	zassert_equal(err, 0,
		      "Test execution hanged waiting for write log ack.");
}

void test_main(void)
{
	ztest_test_suite(storage_controller_tests,
			 ztest_unit_test(test_event_manager_init),
			 ztest_unit_test(test_storage_init),
			 ztest_unit_test(test_storage_append_log_data));
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
		return false;
	}
	return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, stg_write_memrec_event);
EVENT_SUBSCRIBE(test_main, stg_ack_write_memrec_event);
EVENT_SUBSCRIBE(test_main, stg_read_memrec_event);
EVENT_SUBSCRIBE(test_main, stg_ack_read_memrec_event);
