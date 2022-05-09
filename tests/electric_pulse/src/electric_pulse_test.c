/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "ep_event.h"
#include "ep_module.h"
#include "error_event.h"
#include "sound_event.h"

static K_SEM_DEFINE(ep_event_sem, 0, 1);

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

void test_electric_pulse_init(void)
{
	zassert_equal(ep_module_init(), 0, "Error when initializing ep module");
}

void test_electric_pulse_release_early(void)
{
	/* Simulate the highest tone event */
	struct sound_status_event *ev_max = new_sound_status_event();
	ev_max->status = SND_STATUS_PLAYING_MAX;
	EVENT_SUBMIT(ev_max);

	/* Submit event before safety timer expires */
	/* Allocate event. */
	struct ep_status_event *ep_event = new_ep_status_event();
	ep_event->ep_status = EP_RELEASE;
	EVENT_SUBMIT(ep_event);

	int err = k_sem_take(&ep_event_sem, K_SECONDS(5));
	zassert_equal(err, 0, "ep_event_sem hanged.");
}

void test_electric_pulse_release_ready(void)
{
	/* Simulate the highest tone event */
	struct sound_status_event *ev_max = new_sound_status_event();
	ev_max->status = SND_STATUS_PLAYING_MAX;
	EVENT_SUBMIT(ev_max);

	/*Submit a new event but now after safety timer is expired */
	k_sleep(K_SECONDS(6));

	/* Allocate event. */
	struct ep_status_event *ready_ep_event = new_ep_status_event();
	ready_ep_event->ep_status = EP_RELEASE;
	EVENT_SUBMIT(ready_ep_event);

	int err = k_sem_take(&ep_event_sem, K_SECONDS(5));

	/* if no error, the semaphore is not given, and it will therefore expire */
	zassert_equal(err, -EAGAIN, "Error event occured");
}

void test_electric_pulse_release_snd_wrn(void)
{
	k_sem_give(&ep_event_sem);
	/* Simulate the warning event, not highest. */
	struct sound_status_event *ev_warn = new_sound_status_event();
	ev_warn->status = SND_STATUS_PLAYING_WARN;
	EVENT_SUBMIT(ev_warn);

	/* Allocate event. */
	struct ep_status_event *ready_ep_event = new_ep_status_event();
	ready_ep_event->ep_status = EP_RELEASE;
	EVENT_SUBMIT(ready_ep_event);

	int err = k_sem_take(&ep_event_sem, K_SECONDS(5));

	/* Semaphore is given on successful test */
	zassert_equal(err, 0, "ep_event_sem hanged.");
}

void test_main(void)
{
	ztest_test_suite(ep_tests, ztest_unit_test(test_init),
			 ztest_unit_test(test_electric_pulse_init),
			 ztest_unit_test(test_electric_pulse_release_early),
			 ztest_unit_test(test_electric_pulse_release_ready),
			 ztest_unit_test(test_electric_pulse_release_snd_wrn));
	ztest_run_test_suite(ep_tests);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_ep_status_event(eh)) {
		struct ep_status_event *ev = cast_ep_status_event(eh);
		switch (ev->ep_status) {
		case EP_RELEASE:
			// Do nothing here since we wait for eror event to be returned
			break;
		default:
			zassert_unreachable("Unexpected command event.");
		}
	}
	if (is_error_event(eh)) {
		struct error_event *ev = cast_error_event(eh);
		switch (ev->sender) {
		case ERR_EP_MODULE:
			zassert_equal(ev->severity, ERR_SEVERITY_ERROR,
				      "Mismatched severity.");
			zassert_equal(ev->code, -EACCES,
				      "Mismatched error code.");

			// Give sempahore only after error event
			k_sem_give(&ep_event_sem);
			break;

		default:
			zassert_unreachable("Unexpected command event.");
			break;
		}
	}
	return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, ep_status_event);
EVENT_SUBSCRIBE(test_main, error_event);
