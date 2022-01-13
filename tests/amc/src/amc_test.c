/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "amc_handler.h"
#include "sound_event.h"
#include "request_events.h"

enum test_event_id {
	TEST_EVENT_INITIALIZE = 0,
	TEST_EVENT_API_REQ_FUNCTIONS = 1
};
static enum test_event_id cur_id = TEST_EVENT_INITIALIZE;

static K_SEM_DEFINE(sound_event_sem, 0, 1);

static K_SEM_DEFINE(ack_fencedata_sem, 0, 1);
static K_SEM_DEFINE(ack_gnssdata_sem, 0, 1);

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

	/* When we initialize the AMC module, we request GNSS and FENCE data
	 * within the module, which means our event handler will trigger below.
	 * We then have mocked away the storage, GNSS, sound and zap module
	 * so we can simualte dummy fence and GNSS data and subscribe to
	 * sound and zap events to check if the calculations are correct.
	 */
	amc_module_init();

	/* Wait for event_handler to finish it zasserts.
	 * We expect to get an ack for both the fencedata and GNSS data as well
	 * as the sound event.
	 */
	int err = k_sem_take(&ack_fencedata_sem, K_SECONDS(30));
	zassert_equal(err, 0, "Test hanged waiting for fencedata.");

	err = k_sem_take(&ack_gnssdata_sem, K_SECONDS(30));
	zassert_equal(err, 0, "Test hanged waiting for gnssdata.");

	err = k_sem_take(&sound_event_sem, K_SECONDS(30));
	zassert_equal(err, 0, "Test hanged waiting for sound event.");
}

void test_request_api_functions()
{
	submit_request_fencedata();
	submit_request_gnssdata();

	/* Wait for event_handler to finish it zasserts.
	 * We expect to get an ack for both the fencedata and GNSS data as well
	 * as the sound event.
	 */
	int err = k_sem_take(&ack_fencedata_sem, K_SECONDS(30));
	zassert_equal(err, 0, "Test hanged waiting for fencedata.");

	err = k_sem_take(&ack_gnssdata_sem, K_SECONDS(30));
	zassert_equal(err, 0, "Test hanged waiting for gnssdata.");

	err = k_sem_take(&sound_event_sem, K_SECONDS(30));
	zassert_equal(err, 0, "Test hanged waiting for sound event.");
}

static void simulate_dummy_fencedata(struct request_fencedata_event *ev)
{
	/* Here we can check which test we're on, so we can have multiple tests
	 * with multiple dummy FENCE/GNSS data fields.
	 */
	if (cur_id == TEST_EVENT_INITIALIZE) {
		for (int i = 0; i < ev->len; i++) {
			memset(ev->data + i, 0xDE, sizeof(uint8_t));
		}
	} else if (cur_id == TEST_EVENT_API_REQ_FUNCTIONS) {
		for (int i = 0; i < ev->len; i++) {
			memset(ev->data + i, 0xAD, sizeof(uint8_t));
		}
	}

	/* Submit ACK here since data has been written to given pointer area. */
	struct amc_ack_event *ack_e = new_amc_ack_event();
	ack_e->type = AMC_REQ_FENCEDATA;
	EVENT_SUBMIT(ack_e);
}

static void simulate_dummy_gnssdata(struct request_gnssdata_event *ev)
{
	/* Here we can check which test we're on, so we can have multiple tests
	 * with multiple dummy FENCE/GNSS data fields.
	 */
	if (cur_id == TEST_EVENT_INITIALIZE) {
		for (int i = 0; i < ev->len; i++) {
			memset(ev->data + i, 0xDE, sizeof(uint8_t));
		}
	} else if (cur_id == TEST_EVENT_API_REQ_FUNCTIONS) {
		for (int i = 0; i < ev->len; i++) {
			memset(ev->data + i, 0xAD, sizeof(uint8_t));
		}
	}

	/* Submit ACK here since data has been written to given pointer area. */
	struct amc_ack_event *ack_e = new_amc_ack_event();
	ack_e->type = AMC_REQ_GNSSDATA;
	EVENT_SUBMIT(ack_e);
}

static bool event_handler(const struct event_header *eh)
{
	/* Regardless of test, if it is of type request event, we simulate its
	 * contents with our writing dummy function.
	 */
	if (is_request_fencedata_event(eh)) {
		struct request_fencedata_event *ev =
			cast_request_fencedata_event(eh);

		/* Write our own dummy 
		 * data to fencedata that got requested. 
		 */
		simulate_dummy_fencedata(ev);
	}
	if (is_request_gnssdata_event(eh)) {
		struct request_gnssdata_event *ev =
			cast_request_gnssdata_event(eh);

		/* Write our own dummy 
		* data to gnss data that got requested. */
		simulate_dummy_gnssdata(ev);
	}
	/* Give ack semaphores for its respective request type. */
	if (is_amc_ack_event(eh)) {
		struct amc_ack_event *ev = cast_amc_ack_event(eh);
		if (ev->type == AMC_REQ_FENCEDATA) {
			k_sem_give(&ack_fencedata_sem);
		} else if (ev->type == AMC_REQ_GNSSDATA) {
			k_sem_give(&ack_gnssdata_sem);
		} else {
			zassert_unreachable("Unexpected ack ID.");
		}
	}

	/* Checking the sound- and zap events that amc_module outputs based on 
	 * cur_id to check that the amc_module has performed the correct
	 * calculations from our simulated GNSS and FENCE data. For now
	 * both our dummy data results in the SND_WELCOME sound being played.
	 */
	if (cur_id == TEST_EVENT_INITIALIZE) {
		if (is_sound_event(eh)) {
			struct sound_event *ev = cast_sound_event(eh);
			zassert_equal(ev->type, SND_WELCOME,
				      "Unexpected sound event type received");
			/* Test went through, notify test with k_sem_give
			 * and prepare for next test with cur_id. 
			 */
			k_sem_give(&sound_event_sem);
			cur_id = TEST_EVENT_API_REQ_FUNCTIONS;
		}
	} else if (cur_id == TEST_EVENT_API_REQ_FUNCTIONS) {
		if (is_sound_event(eh)) {
			struct sound_event *ev = cast_sound_event(eh);
			zassert_equal(ev->type, SND_WELCOME,
				      "Unexpected sound event type received");
			/* Test went through, notify test with k_sem_give
			 * and prepare for next test with cur_id. 
			 */
			k_sem_give(&sound_event_sem);
		}
	}
	return false;
}

void test_main(void)
{
	ztest_test_suite(amc_tests, ztest_unit_test(test_init),
			 ztest_unit_test(test_request_api_functions));
	ztest_run_test_suite(amc_tests);
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, sound_event);
EVENT_SUBSCRIBE(test_main, request_fencedata_event);
EVENT_SUBSCRIBE(test_main, request_gnssdata_event);
EVENT_SUBSCRIBE(test_main, amc_ack_event);