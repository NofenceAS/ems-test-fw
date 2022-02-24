/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "buzzer.h"
#include "sound_event.h"
#include <event_manager.h>
#include <stdio.h>
#include <string.h>
#include <drivers/pwm.h>
#include "melodies.h"

K_SEM_DEFINE(sound_idle_sem, 0, 1);
K_SEM_DEFINE(sound_playing_sem, 0, 1);
K_SEM_DEFINE(sound_playing_max_sem, 0, 1);

void test_init_event_manager(void)
{
	zassert_false(event_manager_init(),
		      "Error when initializing event manager");
}
void test_init_buzzer(void)
{
	ztest_returns_value(pwm_get_cycles_per_sec, 0);
	zassert_false(buzzer_module_init(), "Error when initializing buzzer");

	/* Should publish sound IDLE event, wait for the semaphore. */
	int err = k_sem_take(&sound_idle_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");
}

void test_perspelmann(void)
{
	/* Check if we get the expected song. */
	for (int i = 0; i < n_perspelmann_notes; i++) {
		uint32_t freq = m_perspelmann[i].t;
		ztest_returns_value(pwm_pin_set_usec, 0);
		ztest_expect_value(pwm_pin_set_usec, pulse, freq / 2);
		ztest_expect_value(pwm_pin_set_usec, period, freq);

		/* Expect to set to 0 after note was played. */
		ztest_returns_value(pwm_pin_set_usec, 0);
		ztest_expect_value(pwm_pin_set_usec, pulse, 0);
		ztest_expect_value(pwm_pin_set_usec, period, 0);
	}

	struct sound_event *ev = new_sound_event();
	ev->type = SND_PERSPELMANN;
	EVENT_SUBMIT(ev);

	/* Wait for playing event, then idle event when it finishes. */
	int err = k_sem_take(&sound_playing_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");

	err = k_sem_take(&sound_idle_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");
}

/** @brief Tests if the first sound played is aborted due to higher priority
 *         requested on second event publish.
 *
*/
void test_priority_second_prio(void)
{
	/* Check if we get the expected song FIND_ME. */
	for (int i = 0; i < n_geiterams_notes; i++) {
		int freq = m_geiterams[i].t;
		ztest_returns_value(pwm_pin_set_usec, 0);
		ztest_expect_value(pwm_pin_set_usec, pulse, freq / 2);
		ztest_expect_value(pwm_pin_set_usec, period, freq);

		/* Expect to set to 0 again. */
		ztest_returns_value(pwm_pin_set_usec, 0);
		ztest_expect_value(pwm_pin_set_usec, pulse, 0);
		ztest_expect_value(pwm_pin_set_usec, period, 0);
	}

	struct sound_event *ev = new_sound_event();
	ev->type = SND_PERSPELMANN;
	EVENT_SUBMIT(ev);

	/* Uncommenting this will result in expected error that proves
	 * it's working as expected. However, it's not possible to
	 * negative assert ztest_expect_value if we get unusued param,
	 * but it shows that the first sound is played as 
	 * expected, and then overwritten by the next request.
	 */
	//k_sleep(K_SECONDS(5));

	struct sound_event *ev_find_me = new_sound_event();
	ev_find_me->type = SND_FIND_ME;
	EVENT_SUBMIT(ev_find_me);

	/* Wait for playing event, then idle event when it finishes. */
	int err = k_sem_take(&sound_playing_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");

	err = k_sem_take(&sound_idle_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");
}

/** @brief Tests if the first sound is NOT aborted due 
 *         to higher priority being played
 *         already. 
 */
void test_priority_first_prio(void)
{
	/* Check if we get the expected song FIND_ME. */
	for (int i = 0; i < n_geiterams_notes; i++) {
		int freq = m_geiterams[i].t;
		ztest_returns_value(pwm_pin_set_usec, 0);
		ztest_expect_value(pwm_pin_set_usec, pulse, freq / 2);
		ztest_expect_value(pwm_pin_set_usec, period, freq);

		/* Expect to set to 0 again. */
		ztest_returns_value(pwm_pin_set_usec, 0);
		ztest_expect_value(pwm_pin_set_usec, pulse, 0);
		ztest_expect_value(pwm_pin_set_usec, period, 0);
	}

	struct sound_event *ev_find_me = new_sound_event();
	ev_find_me->type = SND_FIND_ME;
	EVENT_SUBMIT(ev_find_me);
	struct sound_event *ev = new_sound_event();
	ev->type = SND_PERSPELMANN;
	EVENT_SUBMIT(ev);

	/* Wait for playing event, then idle event when it finishes. */
	int err = k_sem_take(&sound_playing_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");

	err = k_sem_take(&sound_idle_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");
}

void setup_common(void)
{
	/* Take all semaphores to ensure they need
	 * to be given from the tested module prior to the tests that need them. 
	 */
	k_sem_take(&sound_idle_sem, K_SECONDS(30));
	k_sem_take(&sound_playing_sem, K_SECONDS(30));
	k_sem_take(&sound_playing_max_sem, K_SECONDS(30));
}

void teardown_common(void)
{
	k_sleep(K_SECONDS(30));
}

void test_main(void)
{
	ztest_test_suite(
		buzzer_tests, ztest_unit_test(test_init_event_manager),
		ztest_unit_test_setup_teardown(test_init_buzzer, setup_common,
					       teardown_common),
		ztest_unit_test_setup_teardown(test_perspelmann, setup_common,
					       teardown_common),
		ztest_unit_test_setup_teardown(test_priority_second_prio,
					       setup_common, teardown_common),
		ztest_unit_test_setup_teardown(test_priority_first_prio,
					       setup_common, teardown_common));

	ztest_run_test_suite(buzzer_tests);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_sound_status_event(eh)) {
		struct sound_status_event *ev = cast_sound_status_event(eh);
		if (ev->status == SND_STATUS_IDLE) {
			k_sem_give(&sound_idle_sem);
		} else if (ev->status == SND_STATUS_PLAYING) {
			k_sem_give(&sound_playing_sem);
		} else if (ev->status == SND_STATUS_PLAYING_MAX) {
			k_sem_give(&sound_playing_max_sem);
		}
		return false;
	}
	zassert_true(false, "Unexpected event type received");
	return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, sound_status_event);
