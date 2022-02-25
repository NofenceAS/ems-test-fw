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

/* This test tries to run the warn_zone event without setting the
 * frequency event first. This results in the sound controller to not
 * play anything since it sees the frequency is outside the
 * warn zone frequency range (0) and exits.
 */
void test_warn_zone_invalid_freq_event(void)
{
	struct sound_event *ev = new_sound_event();
	ev->type = SND_WARN;
	EVENT_SUBMIT(ev);

	/* We should not get playing event since it never starts playing due
	 * to 0 as the warn zone frequency. 
	 */
	int err = k_sem_take(&sound_playing_sem, K_SECONDS(30));
	zassert_not_equal(err, 0, "");

	/* Take error_timeout_sem, we should be given this, as we never
	 * give the sound controller a frequency warn zone update.
	 */
	err = k_sem_take(&error_timeout_sem, K_SECONDS(30));
	zassert_equal(err, 0, "Should not timeout in this test.");
}

/* Tests that we can set a frequency to the warn zone variable, but we will
 * timeout, since we do not update it within the necessary timeout interval
 * a second time. Also tests to set frequency before SND_WARN event
 * and see the correct frequency is still played.
 */
void test_warn_zone_timeout(void)
{
	ztest_returns_value(pwm_pin_set_usec, 0);
	ztest_expect_value(pwm_pin_set_usec, pulse,
			   WARN_FREQ_MS_PERIOD_MAX / 2);
	ztest_expect_value(pwm_pin_set_usec, period, WARN_FREQ_MS_PERIOD_MAX);

	/* Turned off. */
	ztest_returns_value(pwm_pin_set_usec, 0);
	ztest_expect_value(pwm_pin_set_usec, pulse, 0);
	ztest_expect_value(pwm_pin_set_usec, period, 0);

	struct sound_set_warn_freq_event *ef = new_sound_set_warn_freq_event();
	ef->freq = WARN_FREQ_MS_PERIOD_MAX;
	EVENT_SUBMIT(ef);

	struct sound_event *ev = new_sound_event();
	ev->type = SND_WARN;
	EVENT_SUBMIT(ev);

	/* Wait for playing MAX event. */
	int err = k_sem_take(&sound_playing_max_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");

	/* Wait for playing idle event. */
	err = k_sem_take(&sound_idle_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");

	/* Take error_timeout_sem, we should be given this, as we expect a
	 * timeout.
	 */
	err = k_sem_take(&error_timeout_sem, K_SECONDS(30));
	zassert_equal(err, 0, "Should timeout in this test.");
}

/* Plays the warn zone until we give sound controller a frequency
 * that's out of range.
 */
void test_warn_zone_play_until_range(void)
{
	int err;
	/* Tell the sound controller we're in the warn zone, we have 
	 * 1 second to give it a value with the frequency event. But we
	 * still have to reset the sound controller global frequency, 
	 * unfortunately since it has been set from previous test.
	 */
	struct sound_set_warn_freq_event *efw = new_sound_set_warn_freq_event();
	efw->freq = 0;
	EVENT_SUBMIT(efw);

	err = k_sem_take(&freq_event_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");

	k_sleep(K_SECONDS(30));

	struct sound_event *ev = new_sound_event();
	ev->type = SND_WARN;
	EVENT_SUBMIT(ev);

	int curr_freq = WARN_FREQ_MS_PERIOD_INIT;
	for (int i = curr_freq; i >= WARN_FREQ_MS_PERIOD_MAX; i--) {
		/* PWM turned on. */
		ztest_returns_value(pwm_pin_set_usec, 0);
		ztest_expect_value(pwm_pin_set_usec, pulse, i / 2);
		ztest_expect_value(pwm_pin_set_usec, period, i);

		/* PWM turned off. */
		ztest_returns_value(pwm_pin_set_usec, 0);
		ztest_expect_value(pwm_pin_set_usec, pulse, 0);
		ztest_expect_value(pwm_pin_set_usec, period, 0);

		struct sound_set_warn_freq_event *ef =
			new_sound_set_warn_freq_event();
		ef->freq = i;
		EVENT_SUBMIT(ef);

		/* Wait until we know sound_controller har recieved the event,
		 * then wait some ms to play the frequency for some random time.
		 */
		err = k_sem_take(&freq_event_sem, K_SECONDS(30));
		zassert_equal(err, 0, "");

		k_sleep(K_MSEC(20));
	}

	struct sound_event *evs = new_sound_event();
	evs->type = SND_OFF;
	EVENT_SUBMIT(evs);

	err = k_sem_take(&sound_playing_sem, K_SECONDS(120));
	zassert_equal(err, 0, "");

	/* Wait for playing MAX event, we should get it once
	 * since we iterate ONCE through all frequencies from MIN to MAX. */
	err = k_sem_take(&sound_playing_max_sem, K_SECONDS(120));
	zassert_equal(err, 0, "");

	/* Wait for playing idle event. */
	err = k_sem_take(&sound_idle_sem, K_SECONDS(120));
	zassert_equal(err, 0, "");

	/* Take error_timeout_sem, we should not be given this, as we do not
	 * expect a timeout.
	 */
	err = k_sem_take(&error_timeout_sem, K_SECONDS(120));
	zassert_not_equal(err, 0, "");
}