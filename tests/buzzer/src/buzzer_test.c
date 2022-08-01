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

#include "buzzer_test.h"

#include "error_event.h"

K_SEM_DEFINE(sound_idle_sem, 0, 1);
K_SEM_DEFINE(sound_playing_sem, 0, 1);
K_SEM_DEFINE(sound_playing_warn_sem, 0, 1);
K_SEM_DEFINE(sound_playing_max_sem, 0, 1);

K_SEM_DEFINE(error_timeout_sem, 0, 1);
K_SEM_DEFINE(freq_event_sem, 0, 1);

/** @brief Helper function for expecting pulse and period as pwm_pin_set. 
 *         Expects duty cycle of 50%. Also always expects pwm pins to be
 *         set to 0 again after being played.
*/
void mock_expect_pwm_hz(uint32_t freq)
{
	uint32_t period = freq == 0 ? 0 : 1000000 / freq;
	/* Expect to set to 0 after note was played. */
	ztest_returns_value(pwm_pin_set_usec, 0);
	ztest_expect_value(pwm_pin_set_usec, pulse, period / 2);
	ztest_expect_value(pwm_pin_set_usec, period, period);

	ztest_returns_value(pwm_pin_set_usec, 0);
	ztest_expect_value(pwm_pin_set_usec, pulse, 0);
	ztest_expect_value(pwm_pin_set_usec, period, 0);
}

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
		mock_expect_pwm_hz(m_perspelmann[i].t);
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

void test_find_me(void)
{
	/* Check if we get the expected song. */
	for (int i = 0; i < n_geiterams_notes; i++) {
		mock_expect_pwm_hz(m_geiterams[i].t);
	}

	struct sound_event *ev = new_sound_event();
	ev->type = SND_FIND_ME;
	EVENT_SUBMIT(ev);

	/* Wait for playing event, then idle event when it finishes. */
	int err = k_sem_take(&sound_playing_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");

	err = k_sem_take(&sound_idle_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");
}

void test_setupmode(void)
{
	/* Check if we get the expected song. */
	for (int i = 0; i < n_setupmode_notes; i++) {
		mock_expect_pwm_hz(m_setupmode[i].t);
	}

	struct sound_event *ev = new_sound_event();
	ev->type = SND_SETUPMODE;
	EVENT_SUBMIT(ev);

	/* Wait for playing event, then idle event when it finishes. */
	int err = k_sem_take(&sound_playing_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");

	err = k_sem_take(&sound_idle_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");
}

void test_play_cattle(void)
{
	int i = 250;
	mock_expect_pwm_hz(i);

	for (; i < 400; i += 1) {
		mock_expect_pwm_hz(i);
	}

	mock_expect_pwm_hz(i);

	for (; i > 250; i -= 3) {
		mock_expect_pwm_hz(i);
	}

	mock_expect_pwm_hz(i);

	struct sound_event *ev = new_sound_event();
	ev->type = SND_CATTLE;
	EVENT_SUBMIT(ev);

	/* Wait for playing event, then idle event when it finishes. */
	int err = k_sem_take(&sound_playing_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");

	err = k_sem_take(&sound_idle_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");
}

void test_short_100(void)
{
	mock_expect_pwm_hz(1000);

	struct sound_event *ev = new_sound_event();
	ev->type = SND_SHORT_100;
	EVENT_SUBMIT(ev);

	/* Wait for playing event, then idle event when it finishes. */
	int err = k_sem_take(&sound_playing_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");

	err = k_sem_take(&sound_idle_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");
}

void test_short_200(void)
{
	mock_expect_pwm_hz(100);

	struct sound_event *ev = new_sound_event();
	ev->type = SND_SHORT_200;
	EVENT_SUBMIT(ev);

	/* Wait for playing event, then idle event when it finishes. */
	int err = k_sem_take(&sound_playing_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");

	err = k_sem_take(&sound_idle_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");
}

void test_solar_test(void)
{
	mock_expect_pwm_hz(tone_c_6);
	mock_expect_pwm_hz(0);
	mock_expect_pwm_hz(tone_g_6);

	struct sound_event *ev = new_sound_event();
	ev->type = SND_SOLAR_TEST;
	EVENT_SUBMIT(ev);

	/* Wait for playing event, then idle event when it finishes. */
	int err = k_sem_take(&sound_playing_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");

	err = k_sem_take(&sound_idle_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");
}

void test_off(void)
{
	ztest_returns_value(pwm_pin_set_usec, 0);
	ztest_expect_value(pwm_pin_set_usec, pulse, 0);
	ztest_expect_value(pwm_pin_set_usec, period, 0);

	struct sound_event *ev = new_sound_event();
	ev->type = SND_OFF;
	EVENT_SUBMIT(ev);

	int err = k_sem_take(&sound_idle_sem, K_SECONDS(30));
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
		mock_expect_pwm_hz(m_geiterams[i].t);
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
		mock_expect_pwm_hz(m_geiterams[i].t);
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
	k_sem_take(&error_timeout_sem, K_SECONDS(30));
	k_sem_take(&freq_event_sem, K_SECONDS(30));
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
		ztest_unit_test_setup_teardown(test_find_me, setup_common,
					       teardown_common),
		ztest_unit_test_setup_teardown(test_setupmode, setup_common,
					       teardown_common),
		ztest_unit_test_setup_teardown(test_play_cattle, setup_common,
					       teardown_common),
		ztest_unit_test_setup_teardown(test_short_100, setup_common,
					       teardown_common),
		ztest_unit_test_setup_teardown(test_short_200, setup_common,
					       teardown_common),
		ztest_unit_test_setup_teardown(test_solar_test, setup_common,
					       teardown_common),
		ztest_unit_test_setup_teardown(test_off, setup_common,
					       teardown_common),
		ztest_unit_test_setup_teardown(test_priority_second_prio,
					       setup_common, teardown_common),
		ztest_unit_test_setup_teardown(test_priority_first_prio,
					       setup_common, teardown_common),
		ztest_unit_test_setup_teardown(test_warn_zone_init,
					       setup_common, teardown_common),
		ztest_unit_test_setup_teardown(test_warn_zone_timeout,
					       setup_common, teardown_common),
		ztest_unit_test_setup_teardown(test_warn_zone_play_until_range,
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
		} else if (ev->status == SND_STATUS_PLAYING_WARN) {
			k_sem_give(&sound_playing_warn_sem);
		} else if (ev->status == SND_STATUS_PLAYING_MAX) {
			k_sem_give(&sound_playing_max_sem);
		}
		return false;
	}
	if (is_error_event(eh)) {
		struct error_event *ev = cast_error_event(eh);
		if (ev->code == -ETIMEDOUT) {
			k_sem_give(&error_timeout_sem);
		}
		return false;
	}
	if (is_sound_set_warn_freq_event(eh)) {
		k_sem_give(&freq_event_sem);
		return false;
	}
	zassert_true(false, "Unexpected event type received");
	return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, sound_status_event);
EVENT_SUBSCRIBE(test_main, error_event);
EVENT_SUBSCRIBE_FINAL(test_main, sound_set_warn_freq_event);