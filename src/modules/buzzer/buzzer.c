/*
 * Copyright (c) 2021 Nofence AS
 */

#include <zephyr.h>
#include <logging/log.h>
#include "sound_event.h"
#include "buzzer.h"
#include <device.h>
#include <drivers/pwm.h>
#include "melodies.h"
#include "pwr_module.h"

#include "error_event.h"
#include <drivers/gpio.h>

#define MODULE buzzer
LOG_MODULE_REGISTER(MODULE, CONFIG_BUZZER_LOG_LEVEL);

#if DT_NODE_HAS_STATUS(DT_ALIAS(pwm_buzzer), okay)
#define PWM_BUZZER_NODE DT_ALIAS(pwm_buzzer)
#define PWM_BUZZER_LABEL DT_GPIO_LABEL(PWM_BUZZER_NODE, pwms)
#define BUZZER_PWM_CHANNEL DT_PWMS_CHANNEL(DT_ALIAS(pwm_buzzer))
#endif

static const struct device *buzzer_pwm;
static const struct device *buzzer_pin_dev;
/* Variable used to play sweep sounds, taken from old code. */
#define SWEEP_TIME_PER_STEP_USEC 30

atomic_t current_warn_zone_freq = ATOMIC_INIT(0);
atomic_t current_type_signal = ATOMIC_INIT(SND_READY_FOR_NEXT_TYPE);

static struct k_work_q sound_q;
static struct k_work sound_work;
K_THREAD_STACK_DEFINE(sound_buzzer_area, CONFIG_BUZZER_THREAD_SIZE);

K_SEM_DEFINE(abort_sound_sem, 0, 1);

void warn_zone_timeout_handler(struct k_timer *dummy);
K_TIMER_DEFINE(warn_zone_timeout_timer, warn_zone_timeout_handler, NULL);

/** @todo Check power consumption for this PWM set. */
int set_pwm_to_idle(void)
{
	int err = pwm_pin_set_usec(buzzer_pwm, BUZZER_PWM_CHANNEL, 0, 0, 0);
	if (err) {
		LOG_ERR("pwm off fails %i", err);
		return err;
	}
	return 0;
}

/** @brief Give semaphore to abort sounds. Will make the 
 *         k_sem_take call return a value equal 0, in which case
 *         the sound thread returns -EINTR, to be able to notify
 *         that we want to stop playing any further sounds from current event.
*/
static void inline end_current_sound(void)
{
	k_sem_give(&abort_sound_sem);
}

/** @brief Calculates the necessary duty cycle to receive the wanted volume
 *         level percent given, and outputs pulse width.
 * 
 * @param[in] period microseconds period of wanted frequency.
 * @param[in] volume loudness of the played frequency, ranging from 0-100%
 *            This is simply having a ratio for a duty cycle between 0%-50%.
 * 
 * @return pulse width to output target loudness level.
 */
static inline uint32_t get_pulse_width(uint32_t period, uint8_t volume)
{
	return (period / 2) * ((float)volume / 100);
}

/** @brief Plays a frequency for given duration.
 * 
 * @param[in] period period delay in us of pulses.
 * @param[in] sustain length/duration of the note/frequency to be played in us.
 *            If UINT32_MAX, sound is played infinite.
 * @param[in] volume loudness of the played frequency, ranging from 0%-100%
 *            This is simply having a ratio for a duty cycle between 0%-50%.
 * 
 * @return 0 on success, otherwise negative errno.
 * @return -EINTR if sound was aborted by another thread.
 */
int play_from_ms(const uint32_t period, const uint32_t sustain, const uint8_t volume)
{
	int err;

	uint32_t pulse = get_pulse_width(period, volume);

	err = pwm_pin_set_usec(buzzer_pwm, BUZZER_PWM_CHANNEL, period, pulse, 0);
	if (err) {
		LOG_ERR("Error %d: failed to set pulse width", err);
		return err;
	}

	/* Clamp sustain to config to prevent semaphores to timeout
	 * during the note being played. If MAX, we want infinite duration.
	 */
	k_timeout_t dur;
	if (sustain == UINT32_MAX) {
		dur = K_FOREVER;
	} else {
		dur = K_USEC(MIN(sustain, USEC_PER_SEC * CONFIG_BUZZER_LONGEST_NOTE_SUSTAIN - 1));
	}

	int pwm_idle_err;
	if (k_sem_take(&abort_sound_sem, dur) == 0) {
		err = -EINTR;
	}

	pwm_idle_err = set_pwm_to_idle();
	if (pwm_idle_err) {
		return pwm_idle_err;
	}
	return err;
}

/** @brief Helper function to convert hz frequency to ms delay used by
 *         pwm_pin_set_usec.
 * 
 * @param[in] freq frequency of the note to be played in HZ. 0 = off.
 * @param[in] sustain length/duration of the note/frequency to be played in us.
 *            If UINT32_MAX, sound is played infinite.
 * @param[in] volume loudness of the played frequency, ranging from 0%-100%
 *            This is simply having a ratio for a duty cycle between 0%-50%.
 * 
 * @return 0 on success
 *         -EINTR if sound thread aborted.
 *         Otherwise negative errno.
 */
int play_hz(const uint32_t freq, const uint32_t sustain, const uint8_t volume)
{
	uint32_t us_period = freq == 0 ? 0 : USEC_PER_SEC / freq;
	return play_from_ms(us_period, sustain, volume);
}

/** @brief Plays a set of frequencies from start to end with steps. Accepts
 *         sweeps in both directions (start>end or start<end)
 * 
 * @param[in] start_freq start frequency of sweep in hz.
 * @param[in] end_freq end frequency of sweep in hz.
 * @param[in] duration duration of sweep in us.
 * @param[in] step_count number of steps to reach end_freq from start_freq.
 * 
 * @return 0 on success, otherwise negative errno.
 * @return -EINTR if sound was aborted by another thread.
 */
int play_sweep(uint32_t start_freq, uint32_t end_freq, uint32_t duration, uint16_t step_count)
{
	if (step_count == 0 || start_freq == 0 || end_freq == 0 || duration == 0) {
		return -EINVAL;
	}

	uint32_t sustain = duration / step_count;

	for (uint16_t i = 0; i < step_count; i++) {
		uint32_t freq = (uint32_t)(
			start_freq + (int32_t)(i * (((int32_t)end_freq - (int32_t)start_freq) /
						    (int32_t)step_count)));

		int err = play_hz(freq, sustain, BUZZER_SOUND_VOLUME_PERCENT);
		if (err) {
			return err;
		}
	}

	return 0;
}

void play_song(const note_t *song, const size_t num_notes)
{
	for (int i = 0; i < num_notes; i++) {
		int err =
			play_hz(song[i].t, song[i].s * USEC_PER_MSEC, BUZZER_SOUND_VOLUME_PERCENT);
		if (err) {
			LOG_WRN("Song aborted.");
			return;
		}
	}
}

void play_cattle(void)
{
	uint16_t i = 250;
	int err = play_hz(i, 10 * USEC_PER_MSEC, BUZZER_SOUND_VOLUME_PERCENT);

	if (err) {
		return;
	}

	for (; i < 400; i += 1) {
		err = play_hz(i, 4000, BUZZER_SOUND_VOLUME_PERCENT);
		if (err) {
			return;
		}
	}

	err = play_hz(i, 500 * USEC_PER_MSEC, BUZZER_SOUND_VOLUME_PERCENT);
	if (err) {
		return;
	}

	for (; i > 250; i -= 3) {
		err = play_hz(i, 4000, BUZZER_SOUND_VOLUME_PERCENT);
		if (err) {
			return;
		}
	}

	err = play_hz(i, 30 * USEC_PER_MSEC, BUZZER_SOUND_VOLUME_PERCENT);
	if (err) {
		return;
	}
}

#define WELCOME_SPEED_MULTIPLE_HACK 3
void play_welcome(void)
{
	int err = play_hz(1000, 10 * USEC_PER_MSEC, BUZZER_SOUND_VOLUME_PERCENT);

	if (err) {
		return;
	}

	int steps = 750 / WELCOME_SPEED_MULTIPLE_HACK;
	err = play_sweep(1000, 4000, SWEEP_TIME_PER_STEP_USEC * steps, steps);

	if (err) {
		return;
	}

	err = play_hz(4000, 30 * USEC_PER_MSEC, BUZZER_SOUND_VOLUME_PERCENT);

	if (err) {
		return;
	}

	err = play_hz(0, 70 * USEC_PER_MSEC, BUZZER_SOUND_VOLUME_PERCENT);

	if (err) {
		return;
	}

	steps = 1650 / WELCOME_SPEED_MULTIPLE_HACK;
	err = play_sweep(700, 4000, SWEEP_TIME_PER_STEP_USEC * steps, steps);

	if (err) {
		return;
	}

	err = play_hz(4000, 30 * USEC_PER_MSEC, BUZZER_SOUND_VOLUME_PERCENT);

	if (err) {
		return;
	}

	steps = 2500 / WELCOME_SPEED_MULTIPLE_HACK;
	err = play_sweep(4000, 1500, SWEEP_TIME_PER_STEP_USEC * steps, steps);

	if (err) {
		return;
	}

	steps = 450 / WELCOME_SPEED_MULTIPLE_HACK;
	err = play_sweep(1500, 600, SWEEP_TIME_PER_STEP_USEC * steps, steps);

	if (err) {
		return;
	}

	err = play_hz(600, 30 * USEC_PER_MSEC, BUZZER_SOUND_VOLUME_PERCENT);

	if (err) {
		return;
	}
}

void play_short_200(void)
{
	play_hz(100, 200 * USEC_PER_MSEC, BUZZER_SOUND_VOLUME_PERCENT);
}

void play_short_100(void)
{
	play_hz(1000, 100 * USEC_PER_MSEC, BUZZER_SOUND_VOLUME_PERCENT);
}

void play_solar_test(void)
{
	int err = play_hz(tone_c_6, 100 * USEC_PER_MSEC, BUZZER_SOUND_VOLUME_PERCENT);
	if (err) {
		return;
	}

	err = play_hz(0, 40 * USEC_PER_MSEC, BUZZER_SOUND_VOLUME_PERCENT);
	if (err) {
		return;
	}

	err = play_hz(tone_g_6, 300 * USEC_PER_MSEC, BUZZER_SOUND_VOLUME_PERCENT);
	if (err) {
		return;
	}
}

/* Deprecated Beep_SNDs?
void play_snd_key(void) 
{
	play_hz(3000, 100 * USEC_PER_MSEC, BUZZER_SOUND_VOLUME_PERCENT);
}

void play_snd_longkey(void) 
{
	int err = play_hz(500, 30 * USEC_PER_MSEC, BUZZER_SOUND_VOLUME_PERCENT);

	if (err) {
		return;
	}

	err = play_hz(200, 10 * USEC_PER_MSEC, BUZZER_SOUND_VOLUME_PERCENT);

	if (err) {
		return;
	}

	err = play_hz(500, 50 * USEC_PER_MSEC, BUZZER_SOUND_VOLUME_PERCENT);

	if (err) {
		return;
	}
}

void play_snd_stop(void) 
{
	play_hz(450, 15 * USEC_PER_MSEC, BUZZER_SOUND_VOLUME_PERCENT);
}

void play_snd_short(void) 
{
	play_hz(100, 1 * USEC_PER_MSEC, BUZZER_SOUND_VOLUME_PERCENT);
}

void play_snd_shutdown(void) 
{
	int i;
	int err;

	for (i = 4500; i > 500; i--) {
		err = play_hz(i, 0, BUZZER_SOUND_VOLUME_PERCENT);
		if (err) {
			return;
		}
	}

	err = play_hz(i, 100 * USEC_PER_MSEC, BUZZER_SOUND_VOLUME_PERCENT);
	if (err) {
		return;
	}
}

void play_snd_prep(void) 
{
	int err = play_hz(500, 20 * USEC_PER_MSEC, BUZZER_SOUND_VOLUME_PERCENT);
	if (err) {
		return;
	}

	err = play_hz(0, 20 * USEC_PER_MSEC, BUZZER_SOUND_VOLUME_PERCENT);
	if (err) {
		return;
	}

	err = play_hz(500, 20 * USEC_PER_MSEC, BUZZER_SOUND_VOLUME_PERCENT);
	if (err) {
		return;
	}
}
*/

/** @brief Timeout function that is called if we do not get a new updated
 *         warn zone frequency within that duration, ultimately ending
 *         the warn zone event.
 */
void warn_zone_timeout_handler(struct k_timer *dummy)
{
	ARG_UNUSED(dummy);

	end_current_sound();

	atomic_set(&current_type_signal, SND_OFF);

	/* Warning here. */
	char *msg = "Timeout on getting a new warn zone freq";
	nf_app_warning(ERR_SOUND_CONTROLLER, -ETIMEDOUT, msg, strlen(msg));
}

/** @brief Plays the frequency forever. The conditions that
 *         will break it is either that:
 * 
 *         1. Another sound event has been queued that has higher priority
 *         2. A new frequency is available.
 *         or
 *         3. AMC timeout for sending periodic frequency update to play
 *            within 1 second occured.
 *        
 * @note The the frequency recevied from AMC must be exactly equal to
 *       WARN_FREQ_MAX in order to publish the
 *       SND_STATUS_PLAYING_MAX event for the EP module to subscribe to
 *       for instance.
 * @return 0 if successful and sound finished.
 *         -ERANGE if frequency is outside MIN, MAX area
 */
int play_warn_zone_from_freq(void)
{
	uint32_t cur_freq = atomic_get(&current_warn_zone_freq);

	if (cur_freq == WARN_FREQ_MAX) {
		/* Submit event to EP that we're now playing
		 * max freq warn zone.
		 */
		struct sound_status_event *ev = new_sound_status_event();
		ev->status = SND_STATUS_PLAYING_MAX;
		EVENT_SUBMIT(ev);
	} else if (cur_freq > WARN_FREQ_MAX || cur_freq < WARN_FREQ_INIT) {
		/* Not a valid frequency, exit entire SND_WARN event. */
		return -ERANGE;
	} else {
		/* Is a valid range, submit warnzone status from this module. 
		 * Can be redundant for each HZ we receive. Remove in future? 
		 */
		struct sound_status_event *ev = new_sound_status_event();
		ev->status = SND_STATUS_PLAYING_WARN;
		EVENT_SUBMIT(ev);
	}

	/* Play the frequency forever, until timeout by either new 
	 * frequency update or AMC freq timeout.
	 */
	play_hz(cur_freq, UINT32_MAX, BUZZER_SOUND_VOLUME_PERCENT);
	return 0;
}

void play()
{
	k_sem_take(&abort_sound_sem, K_NO_WAIT);

	if (buzzer_pwm == NULL) {
		LOG_ERR("Buzzer PWM not yet initialized.");
		return;
	}

	int err = 0;

	/* Set to false indicating we're ready to wait for true signal again. */
	//atomic_set(&stop_sound_signal, false);

	/* Force external clock usage, to avoid frequency changes in case of
	 * concurrent BLE activity. See XF-181 */
	pwr_module_use_extclk(REQ_SOUND_CONTROLLER, true);

	enum sound_event_type type = atomic_get(&current_type_signal);

	if (type != SND_OFF && type != SND_WARN) {
		struct sound_status_event *ev_playing = new_sound_status_event();
		ev_playing->status = SND_STATUS_PLAYING;
		EVENT_SUBMIT(ev_playing);
	}

	switch (type) {
	case SND_OFF: {
		err = set_pwm_to_idle();
		if (err) {
			/* Release external clock usage. See XF-181 */
			pwr_module_use_extclk(REQ_SOUND_CONTROLLER, false);
			return;
		}
		break;
	}
	case SND_WELCOME: {
		play_welcome();
		break;
	}
	case SND_SETUPMODE: {
		play_song(m_setupmode, n_setupmode_notes);
		break;
	}
	case SND_SHORT_100: {
		play_short_100();
		break;
	}
	case SND_SHORT_200: {
		play_short_200();
		break;
	}
	case SND_SOLAR_TEST: {
		play_solar_test();
		break;
	}
	case SND_PERSPELMANN: {
		play_song(m_perspelmann, n_perspelmann_notes);
		break;
	}
	case SND_FIND_ME: {
		play_song(m_geiterams, n_geiterams_notes);
		break;
	}

	case SND_CATTLE: {
		play_cattle();
		break;
	};
	case SND_WARN: {
		/* Loop until we're not in the warn zone anymore.
		 * (Either timeout and we set SND_OFF, or out of freq range.)
		 */
		while (atomic_get(&current_type_signal) == SND_WARN) {
			err = play_warn_zone_from_freq();
			if (err == -ERANGE) {
				break;
			}
		}
		break;
	}
	default: {
		break;
	}
	}

	/* Release external clock usage. See XF-181 */
	pwr_module_use_extclk(REQ_SOUND_CONTROLLER, false);

	struct sound_status_event *ev_idle = new_sound_status_event();
	ev_idle->status = SND_STATUS_IDLE;
	EVENT_SUBMIT(ev_idle);

	atomic_set(&current_type_signal, SND_READY_FOR_NEXT_TYPE);
	k_timer_stop(&warn_zone_timeout_timer);
}

int buzzer_module_init(void)
{
	uint64_t cycles;
	buzzer_pin_dev = device_get_binding("GPIO_0");
	int ret = gpio_pin_configure(buzzer_pin_dev, BUZZER_PWM_CHANNEL,
				     (GPIO_ACTIVE_HIGH | GPIO_DS_ALT_HIGH | GPIO_DS_ALT_LOW));
	if (ret != 0) {
		LOG_ERR("Failed to set GPIO parameters for the buzzer channel"
			".");
	}
	gpio_pin_set(buzzer_pin_dev, BUZZER_PWM_CHANNEL, 0);
	buzzer_pwm = device_get_binding(PWM_BUZZER_LABEL);
	if (!buzzer_pwm) {
		LOG_ERR("Cannot find buzzer PWM device! %s", log_strdup(PWM_BUZZER_LABEL));
		return -ENODEV;
	}

	int err = pwm_get_cycles_per_sec(buzzer_pwm, BUZZER_PWM_CHANNEL, &cycles);
	if (err) {
		LOG_ERR("Error getting clock cycles for PWM %d", err);
		return err;
	}

	k_work_queue_init(&sound_q);
	k_work_queue_start(&sound_q, sound_buzzer_area, K_THREAD_STACK_SIZEOF(sound_buzzer_area),
			   CONFIG_BUZZER_THREAD_PRIORITY, NULL);
	k_work_init(&sound_work, play);

	struct sound_status_event *ev = new_sound_status_event();
	ev->status = SND_STATUS_IDLE;
	EVENT_SUBMIT(ev);

	return 0;
}

/**
 * @brief Main event handler function. 
 * 
 * @param[in] eh Event_header for the if-chain to 
 *               use to recognize which event triggered.
 * 
 * @return True or false based on if we want to consume the event or not.
 */
static bool event_handler(const struct event_header *eh)
{
	if (is_sound_event(eh)) {
		struct sound_event *ev = cast_sound_event(eh);

		/* Check if we want to process or not. Must be done here
		 * since sound thread is busy playing. Notify sound thread
		 * with whether it should stop playing or continue.
		 */
		if (ev->type <= atomic_get(&current_type_signal)) {
			end_current_sound();
			atomic_set(&current_type_signal, ev->type);

			if (ev->type == SND_WARN) {
				/* We entered warn zone, initialize the frequency
				 * to start at minimum and wait for AMC to
				 * update it.
				 */
				atomic_set(&current_warn_zone_freq, WARN_FREQ_INIT);
				/* If current type is warn zone, start timeout timer
			 	 * for getting a new frequency to play. */
				k_timer_start(&warn_zone_timeout_timer,
					      K_SECONDS(CONFIG_BUZZER_UPDATE_WARN_FREQ_TIMEOUT),
					      K_NO_WAIT);
			} else {
				/* Stop any existing timeout timers if we have
				 * another event type than warn zone.
				 */
				k_timer_stop(&warn_zone_timeout_timer);
			}
			k_work_submit_to_queue(&sound_q, &sound_work);
		}
		return false;
	}
	if (is_sound_set_warn_freq_event(eh)) {
		struct sound_set_warn_freq_event *ev = cast_sound_set_warn_freq_event(eh);

		/* Update the frequency played. */
		atomic_set(&current_warn_zone_freq, ev->freq);

		/* Restart play_hz as we have a new freq, if currently
		 * playing SND_WARN and restart timeout timer. Make sure
		 * timer is off if we have other events other than SND_WARN.
		 */
		if (atomic_get(&current_type_signal) == SND_WARN) {
			end_current_sound();
			k_timer_start(&warn_zone_timeout_timer,
				      K_SECONDS(CONFIG_BUZZER_UPDATE_WARN_FREQ_TIMEOUT), K_NO_WAIT);
		} else {
			k_timer_stop(&warn_zone_timeout_timer);
		}
		return false;
	}

	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, sound_event);
EVENT_SUBSCRIBE_EARLY(MODULE, sound_set_warn_freq_event);