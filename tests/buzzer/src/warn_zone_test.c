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
 * frequency event first. We should expect the sound controller to play the
 * init frequency for the timeout duration since we do not give it any updates.
 */
void test_warn_zone_init(void)
{
	mock_expect_pwm_hz(WARN_FREQ_INIT);

	struct sound_event *ev = new_sound_event();
	ev->type = SND_WARN;
	EVENT_SUBMIT(ev);

	/* We should get playing warn event 
	 * since we just issued the event.
	 */
	int err = k_sem_take(&sound_playing_warn_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");

	/* We should timeout here, since we do not get a new frequency update. */
	err = k_sem_take(&error_timeout_sem, K_SECONDS(30));
	zassert_equal(err, 0, "Didn't timeout as expected!");

	/* Finish with playing IDLE. */
	err = k_sem_take(&sound_idle_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");
}

/* Tests that we can set a frequency to the warn zone variable, but we will
 * timeout, since we do not update it within the necessary timeout interval
 * a second time. Also tests to set frequency before SND_WARN event
 * and see the correct frequency is still played.
 */
void test_warn_zone_timeout(void)
{
	/* Issue warn zone, will start playing initial frequency. */
	mock_expect_pwm_hz(WARN_FREQ_INIT);

	struct sound_event *ev = new_sound_event();
	ev->type = SND_WARN;
	EVENT_SUBMIT(ev);

	/* Wait for playing warn zone event. */
	int err = k_sem_take(&sound_playing_warn_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");

	/* Play MAX event. */
	mock_expect_pwm_hz(WARN_FREQ_MAX);

	struct sound_set_warn_freq_event *ef = new_sound_set_warn_freq_event();
	ef->freq = WARN_FREQ_MAX;
	EVENT_SUBMIT(ef);

	/* Wait for playing MAX event. */
	err = k_sem_take(&sound_playing_max_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");

	/* Wait for playing idle event which should happen as we timeout. */
	err = k_sem_take(&sound_idle_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");

	/* Take error_timeout_sem, we should be given this, as we expect a
	 * timeout.
	 */
	err = k_sem_take(&error_timeout_sem, K_SECONDS(30));
	zassert_equal(err, 0, "Should timeout in this test.");
}

/* Plays the warn zone until we give sound controller a frequency
 * that is the SND_MAX. The sound controller should detect that this is the
 * max, so it publishes a SND_MAX status event. Finishes when we pass a
 * frequency that is outside the range.
 */
void test_warn_zone_play_until_range(void)
{
	int err;

	/* Issue new warn zone event. Expect INIT freq to be played. */
	mock_expect_pwm_hz(WARN_FREQ_INIT);

	struct sound_event *ev = new_sound_event();
	ev->type = SND_WARN;
	EVENT_SUBMIT(ev);

	/* Wait for playing warn zone event. */
	err = k_sem_take(&sound_playing_warn_sem, K_SECONDS(30));
	zassert_equal(err, 0, "");

	for (int i = WARN_FREQ_MAX; i > WARN_FREQ_INIT - 100; i -= 50) {
		if (i <= WARN_FREQ_MAX && i >= WARN_FREQ_INIT) {
			/* Only expect mocked values if its within range. */
			mock_expect_pwm_hz(i);
		}

		struct sound_set_warn_freq_event *ef = new_sound_set_warn_freq_event();
		ef->freq = i;
		EVENT_SUBMIT(ef);

		/* Make sure the event has been processed by sound controller. */
		err = k_sem_take(&freq_event_sem, K_SECONDS(30));
		zassert_equal(err, 0, "");
	}

	/* We should have been given this. */
	err = k_sem_take(&sound_playing_warn_sem, K_SECONDS(120));
	zassert_equal(err, 0, "");

	/* Wait for playing MAX event, we should get it once
	 * since we iterate ONCE through all frequencies from MIN to MAX. 
	 */
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
