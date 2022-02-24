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
void test_warn_zone_no_freq_event(void)
{
	struct sound_event *ev = new_sound_event();
	ev->type = SND_WARN;
	EVENT_SUBMIT(ev);

	/* We should not get playing event since it never starts playing due
	 * to 0 as the warn zone frequency. 
	 */
	int err = k_sem_take(&sound_playing_sem, K_SECONDS(30));
	zassert_not_equal(err, 0, "");

	/* Take error_timeout_sem, we should not be given this, as we
	 * do not expect a timeout in this test. 
	 */
	err = k_sem_take(&error_timeout_sem, K_SECONDS(30));
	zassert_not_equal(err, 0, "Should not timeout in this test.");
}

/* Tests that we can set a frequency to the warn zone variable, but we will
 * timeout, since we do not update it within the necessary timeout interval
 * a second time.
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

	/* Wait for playing MAX event, then idle event when it finishes. */
	int err = k_sem_take(&sound_playing_max_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");

	/* Take error_timeout_sem, we should be given this, as we expect a
	 * timeout.
	 */
	err = k_sem_take(&error_timeout_sem, K_SECONDS(30));
	zassert_equal(err, 0, "Should timeout in this test.");
}