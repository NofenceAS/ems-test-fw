/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "error_handler.h"
#include "error_event.h"
#include "pwr_event.h"
#include <stdio.h>
#include <string.h>

static K_SEM_DEFINE(error_event_sem, 0, 1);

enum test_event_id { TEST_EVENT_1 = 0, TEST_EVENT_2 = 1, TEST_EVENT_3 = 2 };
static enum test_event_id cur_id = TEST_EVENT_1;

/* Provide custom assert post action handler to handle the assertion on OOM
 * error in Event Manager.
 */
BUILD_ASSERT(!IS_ENABLED(CONFIG_ASSERT_NO_FILE_INFO));
void assert_post_action(const char *file, unsigned int line)
{
	printk("assert_post_action - file: %s (line: %u)\n", file, line);
}

void test_init(void)
{
	zassert_false(event_manager_init(),
		      "Error when initializing event manager");
}

void test_normal_error_message(void)
{
	char *e_msg = "Simulated fw normal error. Overflow.";

	nf_app_error(ERR_FW_UPGRADE, -ENOMEM, e_msg, strlen(e_msg));

	int err = k_sem_take(&error_event_sem, K_SECONDS(30));
	zassert_equal(err, 0, "Normal error message, test hanged.");
}

void test_fatal_error_no_message(void)
{
	nf_app_fatal(ERR_FW_UPGRADE, -ENODATA, NULL, 0);

	int err = k_sem_take(&error_event_sem, K_SECONDS(30));
	zassert_equal(err, 0, "Fatal error no message, test hanged.");
}

void test_warning_exceed_message(void)
{
	size_t msg_len = CONFIG_ERROR_USER_MESSAGE_SIZE + 1;
	char msg[msg_len];

	nf_app_warning(ERR_FW_UPGRADE, -EINVAL, msg, msg_len);

	int err = k_sem_take(&error_event_sem, K_SECONDS(30));
	zassert_equal(err, 0, "Exceeding message, test hanged.");
}

void test_main(void)
{
	ztest_test_suite(error_handler_tests, ztest_unit_test(test_init),
			 ztest_unit_test(test_normal_error_message),
			 ztest_unit_test(test_fatal_error_no_message),
			 ztest_unit_test(test_warning_exceed_message));

	ztest_run_test_suite(error_handler_tests);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_error_event(eh)) {
		struct error_event *ev = cast_error_event(eh);
		if (cur_id == TEST_EVENT_1) {
			zassert_equal(ev->sender, ERR_FW_UPGRADE,
				      "Mismatched sender.");

			zassert_equal(ev->severity, ERR_SEVERITY_ERROR,
				      "Mismatched severity.");

			zassert_equal(ev->code, -ENOMEM,
				      "Mismatched error code.");

			char *expected_message =
				"Simulated fw normal error. Overflow.";

			zassert_equal(ev->dyndata.size,
				      strlen(expected_message),
				      "Mismatched message length.");

			zassert_mem_equal(ev->dyndata.data, expected_message,
					  ev->dyndata.size,
					  "Mismatched user message.");
			cur_id = TEST_EVENT_2;
		} else if (cur_id == TEST_EVENT_2) {
			zassert_equal(ev->sender, ERR_FW_UPGRADE,
				      "Mismatched sender.");

			zassert_equal(ev->severity, ERR_SEVERITY_FATAL,
				      "Mismatched severity.");

			zassert_equal(ev->code, -ENODATA,
				      "Mismatched error code.");

			zassert_equal(ev->dyndata.size, 0,
				      "Message not empty.");
			cur_id = TEST_EVENT_3;
		} else if (cur_id == TEST_EVENT_3) {
			zassert_equal(ev->sender, ERR_FW_UPGRADE,
				      "Mismatched sender.");

			zassert_equal(ev->severity, ERR_SEVERITY_WARNING,
				      "Mismatched severity.");

			zassert_equal(ev->code, -EINVAL,
				      "Mismatched error code.");

			zassert_equal(ev->dyndata.size, 0,
				      "Message not empty.");
		}
		k_sem_give(&error_event_sem);
		return false;
	}
	zassert_true(false, "Unexpected event type received");
	return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, error_event);
