/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "amc_handler.h"
#include "sound_event.h"
#include "messaging_module_events.h"
#include "pasture_structure.h"
#include "nf_common.h"
#include "request_events.h"

#include "error_event.h"

K_SEM_DEFINE(sound_event_sem, 0, 1);
K_SEM_DEFINE(pasture_ready_sem, 0, 1);
K_SEM_DEFINE(error_event_sem, 0, 1);

/* Provide custom assert post action handler to handle the assertion on OOM
 * error in Event Manager.
 */
BUILD_ASSERT(!IS_ENABLED(CONFIG_ASSERT_NO_FILE_INFO));
void assert_post_action(const char *file, unsigned int line)
{
	printk("assert_post_action - file: %s (line: %u)\n", file, line);
}

static void simulate_dummy_gnssdata(gnss_struct_t *gnss_out)
{
	gnss_out->lat = 1337;
	gnss_out->lon = 1337;
}

void test_init_and_update_pasture(void)
{
	zassert_false(event_manager_init(),
		      "Error when initializing event manager");

	ztest_returns_value(stg_read_pasture_data, 0);
	zassert_false(amc_module_init(), "Error when initializing AMC module");
}

void test_update_pasture()
{
	struct new_fence_available *event = new_new_fence_available();
	EVENT_SUBMIT(event);

	ztest_returns_value(stg_read_pasture_data, 0);
	int err = k_sem_take(&pasture_ready_sem, K_SECONDS(30));
	zassert_equal(err, 0, "Test hanged waiting for pasture ready event.");
}

void test_update_pasture_error()
{
	struct new_fence_available *event = new_new_fence_available();
	EVENT_SUBMIT(event);

	ztest_returns_value(stg_read_pasture_data, -ENODATA);
	int err = k_sem_take(&error_event_sem, K_SECONDS(30));
	zassert_not_equal(err, 0, "Test should hang for error event.");
}

void test_update_gnssdata()
{
	struct gnssdata_event *event = new_gnssdata_event();
	simulate_dummy_gnssdata(&event->gnss);
	EVENT_SUBMIT(event);

	int err = k_sem_take(&sound_event_sem, K_SECONDS(30));
	zassert_equal(err, 0, "Test hanged waiting for sound event.");
}

#define STRESS_TEST_REQUESTS 50
static int stress_test_counter = 0;

void test_stress_gnss_request()
{
	for (int i = 0; i < STRESS_TEST_REQUESTS; i++) {
		/* For each gnss request, we do one calculation where in our
		 * case we expect sound ack and gnss ack, so wait for those
		 * semaphores before sending next request.
		 */
		struct gnssdata_event *event = new_gnssdata_event();
		simulate_dummy_gnssdata(&event->gnss);
		EVENT_SUBMIT(event);

		int err = k_sem_take(&sound_event_sem, K_SECONDS(30));
		zassert_equal(err, 0, "Test hanged waiting for sound event.");
		stress_test_counter++;
	}
	zassert_equal(stress_test_counter, STRESS_TEST_REQUESTS,
		      "Missed request events.");
}

static bool event_handler(const struct event_header *eh)
{
	if (is_sound_event(eh)) {
		struct sound_event *ev = cast_sound_event(eh);
		zassert_equal(ev->type, SND_WELCOME,
			      "Unexpected sound event type received");
		k_sem_give(&sound_event_sem);
	}
	if (is_new_fence_available(eh)) {
		k_sem_give(&pasture_ready_sem);
	}
	if (is_error_event(eh)) {
		struct error_event *ev = cast_error_event(eh);
		zassert_equal(ev->sender, ERR_SENDER_AMC, "Mismatched sender.");

		zassert_equal(ev->severity, ERR_SEVERITY_FATAL,
			      "Mismatched severity.");

		zassert_equal(ev->code, -ENODATA, "Mismatched error code.");

		char *expected_message = "Cannot update pasture cache on AMC.";

		zassert_equal(ev->dyndata.size, strlen(expected_message),
			      "Mismatched message length.");

		zassert_mem_equal(ev->dyndata.data, expected_message,
				  ev->dyndata.size, "Mismatched user message.");
		k_sem_give(&error_event_sem);
	}
	return false;
}

void test_main(void)
{
	ztest_test_suite(amc_tests,
			 ztest_unit_test(test_init_and_update_pasture),
			 ztest_unit_test(test_update_pasture),
			 ztest_unit_test(test_update_pasture_error),
			 ztest_unit_test(test_update_gnssdata),
			 ztest_unit_test(test_stress_gnss_request));
	ztest_run_test_suite(amc_tests);
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, sound_event);
EVENT_SUBSCRIBE(test_main, gnssdata_event);
EVENT_SUBSCRIBE_FINAL(test_main, new_fence_available);