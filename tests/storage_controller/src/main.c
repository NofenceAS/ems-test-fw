/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "storage.h"
#include "pasture_event.h"
#include "storage_helper.h"

#include "error_event.h"

#include "ano_structure.h"
#include "log_structure.h"
#include "pasture_structure.h"

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
	zassert_false(stg_init_storage_controller(),
		      "Error when initializing storage controller.");
	fill_dummy_fence();
}

void test_clear_fcbs(void)
{
	zassert_equal(stg_clear_partition(STG_PARTITION_LOG), 0, "");
	zassert_equal(stg_clear_partition(STG_PARTITION_ANO), 0, "");
	zassert_equal(stg_clear_partition(STG_PARTITION_PASTURE), 0, "");
}

/** @brief Tests what happens when we mix up the request/write calls to
 *         the storage controller, which is bound to happen in the system
 *         since multiple modules operate simultaneously and will 
 *         both request and write at the same time.
 */
void test_pasture_log_ano_write_read(void)
{
}

void test_main(void)
{
	/* Init FCBs and event manager setup. */
	ztest_test_suite(storage_init, ztest_unit_test(test_event_manager_init),
			 ztest_unit_test(test_init));
	ztest_run_test_suite(storage_init);

	/* Test log partition and read/write/consumes. */
	//ztest_test_suite(
	//	storage_log_test,
	//	ztest_unit_test_setup_teardown(test_log_write, setup_common,
	//				       teardown_common),
	//	ztest_unit_test_setup_teardown(test_log_read, setup_common,
	//				       teardown_common),
	//	ztest_unit_test_setup_teardown(test_write_log_exceed,
	//				       setup_common, teardown_common),
	//	ztest_unit_test_setup_teardown(test_read_log_exceed,
	//				       setup_common, teardown_common),
	//	ztest_unit_test_setup_teardown(test_empty_walk_log,
	//				       setup_common, teardown_common),
	//	ztest_unit_test_setup_teardown(test_reboot_persistent_log,
	//				       setup_common, teardown_common));
	//ztest_run_test_suite(storage_log_test);
	//
	///* Test ano partition and read/write/consumes. */
	//ztest_test_suite(
	//	storage_ano_test,
	//	ztest_unit_test_setup_teardown(test_ano_write, setup_common,
	//				       teardown_common),
	//	ztest_unit_test_setup_teardown(test_ano_read, setup_common,
	//				       teardown_common),
	//	ztest_unit_test_setup_teardown(test_reboot_persistent_ano,
	//				       setup_common, teardown_common));
	//ztest_run_test_suite(storage_ano_test);
	//
	///* Test pasture partition and read/write/consumes. */
	ztest_test_suite(storage_pasture_test, ztest_unit_test(test_pasture),
			 ztest_unit_test(test_reboot_persistent_pasture),
			 ztest_unit_test(test_pasture_extended_write_read),
			 ztest_unit_test(test_request_pasture_multiple),
			 ztest_unit_test(test_no_pasture_available));
	ztest_run_test_suite(storage_pasture_test);

	/* Integration testing of all partitions and multiple read/write/consume
	 * across all partitions and all FCBs. 
	 */
	ztest_test_suite(storage_integration, ztest_unit_test(test_clear_fcbs),
			 ztest_unit_test(test_pasture_log_ano_write_read));
	ztest_run_test_suite(storage_integration);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_error_event(eh)) {
		printk("Error detected. \n");
	}
	return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, error_event);
EVENT_SUBSCRIBE(test_main, pasture_ready_event);
