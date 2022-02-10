/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "amc_handler.h"
#include "sound_event.h"
#include "request_events.h"
#include "pasture_event.h"
#include "pasture_structure.h"
#include "storage_events.h"

enum test_event_id { TEST_EVENT_REQUEST_GNSS = 0, TEST_EVENT_STRESS_GNSS = 1 };
static enum test_event_id cur_id = TEST_EVENT_REQUEST_GNSS;

static bool toggle_stress_request = false;

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

fence_t *dummy_fence;

static void free_dummy_pasture()
{
	k_free(dummy_fence);
}

static void simulate_dummy_pasture(struct stg_read_event *ev)
{
	/* Simulate 13 points. */
	size_t len = sizeof(fence_t) + (13 * sizeof(fence_coordinate_t));
	dummy_fence = k_malloc(len);

	/* Write to 13 points on x and y coords. */
	for (int i = 0; i < 13; i++) {
		dummy_fence->p_c[i].s_x_dm = 1337;
		dummy_fence->p_c[i].s_y_dm = 1337;
	}

	dummy_fence->header.n_points = 13;
	dummy_fence->header.e_fence_type = 1;
	dummy_fence->header.us_id = 3;

	/* Publish read ack event and wait for consumed ack, 
	 * only then do we read the next entry in the walk process.
	 */
	struct stg_ack_read_event *event = new_stg_ack_read_event();
	event->partition = STG_PARTITION_PASTURE;
	event->data = (uint8_t *)dummy_fence;
	event->len = len;

	EVENT_SUBMIT(event);
}

static void simulate_dummy_gnssdata(gnss_struct_t *gnss_out)
{
	/* Here we can check which test we're on, so we can have multiple tests
	 * with multiple dummy FENCE/GNSS data fields.
	 */
	if (cur_id == TEST_EVENT_REQUEST_GNSS) {
		gnss_out->lat = 1337;
		gnss_out->lon = 1337;
	} else if (cur_id == TEST_EVENT_STRESS_GNSS) {
		if (toggle_stress_request) {
			gnss_out->lat = 123;
			gnss_out->lon = 123;
		} else {
			gnss_out->lat = 1337;
			gnss_out->lon = 1337;
		}
	}
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
	zassert_equal(err, 0, "Test hanged waiting for pasture ack.");
}

void test_update_pasture()
{
	struct pasture_ready_event *event = new_pasture_ready_event();
	EVENT_SUBMIT(event);

	int err = k_sem_take(&ack_fencedata_sem, K_SECONDS(30));
	zassert_equal(err, 0, "Test hanged waiting for pasture ack.");
}

#define STRESS_TEST_REQUESTS 50
#define STRESS_TEST_HZ 10

static int stress_test_counter = 0;

void test_update_gnssdata()
{
	struct gnssdata_event *event = new_gnssdata_event();
	simulate_dummy_gnssdata(&event->gnss);
	EVENT_SUBMIT(event);

	int err = k_sem_take(&sound_event_sem, K_SECONDS(30));
	zassert_equal(err, 0, "Test hanged waiting for sound event.");
}

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
		k_sleep(K_MSEC(1000 / STRESS_TEST_HZ));
	}
	zassert_equal(stress_test_counter, STRESS_TEST_REQUESTS,
		      "Missed request events.");
}

static bool event_handler(const struct event_header *eh)
{
	/* Regardless of test, if it is of type request event, we simulate its
	 * contents with our writing dummy function.
	 */
	if (is_stg_read_event(eh)) {
		struct stg_read_event *ev = cast_stg_read_event(eh);

		/* Write our own dummy 
		 * data to pasture that got requested. 
		 */
		simulate_dummy_pasture(ev);
	}
	if (is_stg_consumed_read_event(eh)) {
		free_dummy_pasture();
		k_sem_give(&ack_fencedata_sem);
	}

	/* Checking the sound- and zap events that amc_module outputs based on 
	 * cur_id to check that the amc_module has performed the correct
	 * calculations from our simulated GNSS and FENCE data.
	 */
	if (cur_id == TEST_EVENT_REQUEST_GNSS) {
		if (is_sound_event(eh)) {
			struct sound_event *ev = cast_sound_event(eh);
			zassert_equal(ev->type, SND_WELCOME,
				      "Unexpected sound event type received");
			/* Test went through, notify test with k_sem_give
			 * and prepare for next test with cur_id. 
			 */
			k_sem_give(&sound_event_sem);
			cur_id = TEST_EVENT_STRESS_GNSS;
		}
	} else if (cur_id == TEST_EVENT_STRESS_GNSS) {
		if (is_sound_event(eh)) {
			stress_test_counter++;
			struct sound_event *ev = cast_sound_event(eh);
			if (toggle_stress_request) {
				zassert_equal(
					ev->type, SND_FIND_ME,
					"Unexpected sound event type received");
			} else {
				zassert_equal(
					ev->type, SND_WELCOME,
					"Unexpected sound event type received");
			}
			toggle_stress_request = !toggle_stress_request;
			k_sem_give(&sound_event_sem);
		}
	}
	return false;
}

void test_main(void)
{
	ztest_test_suite(amc_tests, ztest_unit_test(test_init),
			 ztest_unit_test(test_update_pasture),
			 ztest_unit_test(test_update_gnssdata),
			 ztest_unit_test(test_stress_gnss_request));
	ztest_run_test_suite(amc_tests);
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, sound_event);
EVENT_SUBSCRIBE(test_main, gnssdata_event);

EVENT_SUBSCRIBE(test_main, stg_read_event);
EVENT_SUBSCRIBE(test_main, stg_consumed_read_event);