/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "fw_upgrade.h"
#include "fw_upgrade_events.h"
#include "error_event.h"
#include <net/fota_download.h>

static K_SEM_DEFINE(test_status, 0, 1);

enum test_event_id {
	TEST_EVENT_INIT = 0,
	TEST_EVENT_START = 1,
	TEST_EVENT_PROGRESS = 2,
	TEST_EVENT_REBOOT = 3,
	TEST_EVENT_ERROR_IN_DL = 4,
	TEST_EVENT_ERROR_START_DL = 5
};

static enum test_event_id cur_id = TEST_EVENT_INIT;

/* Provide custom assert post action handler to handle the assertion on OOM
 * error in Event Manager.
 */
BUILD_ASSERT(!IS_ENABLED(CONFIG_ASSERT_NO_FILE_INFO));
void assert_post_action(const char *file, unsigned int line)
{
	printk("assert_post_action - file: %s (line: %u)\n", file, line);
}

void setup_init(void)
{
	cur_id = TEST_EVENT_INIT;
	ztest_returns_value(fota_download_init, 0);
	k_sem_take(&test_status, K_SECONDS(30));
}

void test_init(void)
{
	zassert_false(event_manager_init(), "Error when initializing event manager");
	zassert_false(fw_upgrade_module_init(), "Error when initializing firmware upgrade module");
	/* See if we get the 'IDLE' flag as it should be after init. */
	int err = k_sem_take(&test_status, K_SECONDS(30));
	zassert_equal(err, 0, "Test status event execution \
		hanged waiting for IDLE.");
}

void setup_start_fota(void)
{
	cur_id = TEST_EVENT_START;
	ztest_returns_value(fota_download_start, 0);
	k_sem_take(&test_status, K_SECONDS(30));
}

void test_start_fota(void)
{
	struct start_fota_event *ev = new_start_fota_event();
	ev->override_default_host = false;
	ev->version = 2001;
	EVENT_SUBMIT(ev);

	/* See if we get the 'REBOOT' flag as it should be after init. */
	int err = k_sem_take(&test_status, K_SECONDS(30));
	zassert_equal(err, 0, "Test status event execution \
		hanged waiting for REBOOT.");
}

void setup_start_fota_error_in_download(void)
{
	cur_id = TEST_EVENT_ERROR_IN_DL;
	ztest_returns_value(fota_download_start, 0);
	k_sem_take(&test_status, K_SECONDS(30));
}

/* Start same fota test, but now the fota_download module outputs error. */
void test_start_fota_error_in_download(void)
{
	struct start_fota_event *ev = new_start_fota_event();
	ev->override_default_host = false;
	ev->version = 2001;
	EVENT_SUBMIT(ev);

	/* See if we get the error DFU status flag. */
	int err = k_sem_take(&test_status, K_SECONDS(30));
	zassert_equal(err, 0, "Test status event execution \
		hanged waiting for error.");
}

void setup_error_start_download(void)
{
	cur_id = TEST_EVENT_ERROR_START_DL;
	ztest_returns_value(fota_download_start, -ENOTCONN);
	k_sem_take(&test_status, K_SECONDS(30));
}

/* Start same fota test, but now the fota_download module outputs error. */
void test_start_fota_error_start_download(void)
{
	struct start_fota_event *ev = new_start_fota_event();
	ev->override_default_host = false;
	ev->version = 2001;
	EVENT_SUBMIT(ev);

	/** Should not see it on the event bus, since we do not want to
	  * hangup the thread. 
	  */
	int err = k_sem_take(&test_status, K_SECONDS(30));
	zassert_not_equal(err, 0, "");
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
	ztest_test_suite(
		firmware_upgrade_tests,
		ztest_unit_test_setup_teardown(test_init, setup_init, teardown_common),
		ztest_unit_test_setup_teardown(test_start_fota, setup_start_fota, teardown_common),
		ztest_unit_test_setup_teardown(test_start_fota_error_in_download,
					       setup_start_fota_error_in_download, teardown_common),
		ztest_unit_test_setup_teardown(test_start_fota_error_start_download,
					       setup_error_start_download, teardown_common));

	ztest_run_test_suite(firmware_upgrade_tests);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_dfu_status_event(eh)) {
		struct dfu_status_event *ev = cast_dfu_status_event(eh);
		if (cur_id == TEST_EVENT_INIT) {
			zassert_equal(ev->dfu_status, DFU_STATUS_IDLE,
				      "Status not idle as it should after init.");
			k_sem_give(&test_status);
			cur_id = TEST_EVENT_START;
		} else if (cur_id == TEST_EVENT_START) {
			zassert_equal(ev->dfu_status, DFU_STATUS_IN_PROGRESS, "");
			zassert_equal(ev->dfu_error, 0, "");
			cur_id = TEST_EVENT_PROGRESS;
			/* Simualte all fragments have been transferred. */
			simulate_callback_event();
		} else if (cur_id == TEST_EVENT_PROGRESS) {
			zassert_equal(ev->dfu_status, DFU_STATUS_SUCCESS_REBOOT_SCHEDULED, "");
			zassert_equal(ev->dfu_error, 0, "");
			cur_id = TEST_EVENT_REBOOT;
			k_sem_give(&test_status);
		} else if (cur_id == TEST_EVENT_ERROR_IN_DL) {
			zassert_equal((int)ev->dfu_status,
				      (int)FOTA_DOWNLOAD_ERROR_CAUSE_DOWNLOAD_FAILED, "");
			k_sem_give(&test_status);
		}
		return false;
	}
	if (is_error_event(eh)) {
		struct error_event *ev = cast_error_event(eh);
		zassert_equal(ev->sender, ERR_FW_UPGRADE, "Wrong sender expected.");

		if (cur_id == TEST_EVENT_ERROR_START_DL) {
			zassert_equal(ev->code, -ENOTCONN, "");
			zassert_equal(ev->severity, ERR_SEVERITY_ERROR, "");

			char *expected = "Unable to start FOTA DL";
			zassert_equal(ev->dyndata.size, strlen(expected), "");
			zassert_mem_equal(ev->dyndata.data, expected, strlen(expected), "");
			k_sem_give(&test_status);
		} else {
			zassert_unreachable("Unexpected error triggered.");
		}
		return false;
	}
	zassert_true(false, "Wrong event type received");
	return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, dfu_status_event);
EVENT_SUBSCRIBE(test_main, error_event);
