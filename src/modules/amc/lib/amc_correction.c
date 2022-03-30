/*
 * Copyright (c) 2022 Nofence AS
 */

#include <zephyr.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(amc_correction, CONFIG_AMC_LIB_LOG_LEVEL);

#include "amc_correction.h"
#include "amc_states.h"
#include "amc_gnss.h"
#include "amc_const.h"

/* For playing sound and fetching freq limits and zapping. */
#include "sound_event.h"
#include "ep_event.h"

/* Function declarations. */
static void correction_start(int16_t mean_dist);
static void correction_end(void);
static void correction_pause(Reason reason, int16_t mean_dist);
static void correction(Mode amc_mode, int16_t mean_dist, int16_t dist_change);

static bool zap_eval_doing;
static uint32_t zap_timestamp;
static uint32_t correction_pause_timestamp;
static uint16_t max_freq_timestamp;
static uint16_t last_warn_freq;

/* Distance from fence where the last warning was started. */
static int16_t last_warn_dist;

/* True from function Correction start to correction_end. 
 * Needed for function Correction to perform anything. 
 */
static uint8_t correction_started = 0;

/* True from function correction start to correction_pause. (Sound is ON). */
static uint8_t correction_warn_on = 0;

static atomic_t buzzer_off = ATOMIC_INIT(true);

static void correction_start(int16_t mean_dist)
{
	uint32_t delta_correction_pause =
		(k_uptime_get_32() / MSEC_PER_SEC) - correction_pause_timestamp;

	if (delta_correction_pause > CORRECTION_PAUSE_MIN_TIME) {
		if (!correction_started) {
			last_warn_freq = WARN_FREQ_INIT;
			last_warn_dist = mean_dist;
			/** @todo log_WriteCorrectionMessage(true); */

			/** @deprecated ??
			 *  g_i16_WarnStartWayP[0] = GPS()->X;
			 *  g_i16_WarnStartWayP[1] = GPS()->Y;
			 */

			LOG_INF("Starting correction.");
		}
		if (!correction_warn_on) {
			/** Typically we would start the buzzer warn sound
			 *  event here, but we can rather submit that
			 *  if buzzer is OFF AND we are sending it a new warn
			 *  frequency.
		 	*/
			/** @todo log_WriteCorrectionMessage(true); */
			increment_warn_count();
			LOG_INF("Incremented warn zone counter.");
		}
		correction_started = 1;
		correction_warn_on = 1;
	}
}

static void correction_end(void)
{
	if (correction_started && !correction_warn_on) {
		/** Turn off the sound buzzer. @note This will stop
		 * any FIND_ME or other sound events as well. 
		 */
		struct sound_event *snd_ev = new_sound_event();
		snd_ev->type = SND_OFF;
		EVENT_SUBMIT(snd_ev);

		last_warn_dist = LIM_WARN_MIN_DM;
		reset_zap_pain_cnt();

		/** @todo log_WriteCorrectionMessage(false); */

		/** @todo????
		 * #ifdef GSMCONNECT_AFTER_WARN
		 * 		gsm_ConnectAndTransfer(
		 * 			GSM_CALL_CORRECTIONEND); // Makes sure the updated data is transferred immediately when correction ends
		 * #endif
		 */
		correction_started = 0;

		LOG_INF("Ended correction.");
	}
}

/** @brief This routine pauses the warning tone and correction process.
 * 
 * @param reason reason for pause.
 * @param mean_dist mean distance calculated from border. Used to update the
 *        new warn area distance reference.
 */
static void correction_pause(Reason reason, int16_t mean_dist)
{
	int16_t dist_add = 0;

	/** Turn off the sound buzzer. @note This will stop
	 * any FIND_ME or other sound events as well. 
	 */
	struct sound_event *snd_ev = new_sound_event();
	snd_ev->type = SND_OFF;
	EVENT_SUBMIT(snd_ev);

	LOG_INF("Paused correction warning due to reason %i.", reason);

	/* [Legacy code] v4.01-0: There was return here before. 
	 * This became wrong because escaped status did not stop correction, 
	 * and therefore did not reset the zap counter.
	 */
	if (correction_warn_on) {
		/** @todo Notify messaging module and server. Reference old code:
		 * SetStatusReason(reason);
		 * const gps_last_fix_struct_t *gpsLastFixStruct = GPS_last_fix();
		 * NofenceMessage
		 * 	msg = { .which_m =
		 * 			NofenceMessage_client_warning_message_tag,
		 * 		.m.client_warning_message = {
		 * 			.xDatePos = proto_getLastKnownDatePos(
		 * 				gpsLastFixStruct),
		 * 			.usDuration = WarnDuration(),
		 * 			.has_sFenceDist = true,
		 * 			.sFenceDist =
		 * 				gpsp_get_inst_dist_to_border() } };
		 * log_WriteNofenceMessage(&msg);
		 */
	}

	switch (reason) {
	case Reason_WARNPAUSEREASON_COMPASS:
	case Reason_WARNPAUSEREASON_ACC:
	case Reason_WARNPAUSEREASON_MOVEBACK:
	case Reason_WARNPAUSEREASON_MOVEBACKNODIST:
	case Reason_WARNPAUSEREASON_NODIST:
		dist_add = (int16_t)_LAST_DIST_ADD;
		break;
	case Reason_WARNPAUSEREASON_BADFIX:
		dist_add = DIST_OFFSET_AFTER_BADFIX;
		break;
	case Reason_WARNPAUSEREASON_MISSGPSDATA:
		dist_add = 0;
		break;
	case Reason_WARNPAUSEREASON_ZAP:
		dist_add = DIST_OFFSET_AFTER_PAIN;
		break;
	case Reason_WARNSTOPREASON_INSIDE:
	case Reason_WARNSTOPREASON_MOVEBACKINSIDE:
	case Reason_WARNSTOPREASON_ESCAPED:
	case Reason_WARNSTOPREASON_MODE:
		dist_add = 1;
		break;
	default:
		dist_add = 0;
		break;
	}

	if (dist_add > 0) {
		/* Makes sure that sound starts over. */
		last_warn_freq = WARN_FREQ_INIT;

		/* If Distance is set, then restart further warning 
		 * mentioned distance from this distance.
		 */
		last_warn_dist = mean_dist + dist_add;
		if (last_warn_dist < LIM_WARN_MIN_DM) {
			last_warn_dist = LIM_WARN_MIN_DM;
		}
	}
	correction_pause_timestamp = k_uptime_get_32();
	correction_warn_on = 0;

	if (reason > Reason_WARNSTOPREASON) {
		correction_end();
	}
}

void update_buzzer_off(bool is_buzzer_off)
{
	atomic_set(&buzzer_off, is_buzzer_off);
}

static void correction(Mode amc_mode, int16_t mean_dist, int16_t dist_change)
{
	/* Variables used in the correction setup, calculation and end. */
	int16_t inc_tone_slope = 0, dec_tone_slope = 0;
	static uint32_t timestamp;

	if (zap_eval_doing) {
		uint16_t delta_zap_eval =
			(k_uptime_get_32() / MSEC_PER_SEC) - zap_timestamp;

		if (delta_zap_eval >= ZAP_EVALUATION_TIME) {
			/** @todo Notify server and log, i.e submit event to 
			 *  the messaging module. Old code ref:
			 * 
			 * NofenceMessage msg = {
			 * 	.which_m = NofenceMessage_client_zap_message_tag,
			 * 	.m.client_zap_message = {
			 * 		.xDatePos = proto_getLastKnownDatePos(gpsLastFix),
			 * 		.has_sFenceDist = true,
			 * 		.sFenceDist = gpsp_get_inst_dist_to_border()
			 * 	}
			 * };
            		 * log_WriteNofenceMessage(&msg);
			 * \* Makes sure the updated data is 
			 *  * transferred immediately after every zap.
			 *  *\
			 * gsm_SetStatusToConnectAndTransfer(GSM_CALL_CORRECTIONZAP, false);
			 */
			zap_eval_doing = false;
		}
		return;
	} else if (correction_started) {
		if (correction_warn_on) {
			if (last_warn_freq < WARN_FREQ_INIT) {
				last_warn_freq = WARN_FREQ_INIT;
			}

			uint32_t delta_timestamp =
				k_uptime_get_32() - timestamp;
			if (delta_timestamp > WARN_TONE_SPEED_MS) {
				timestamp = k_uptime_get_32();

				if (amc_mode == Mode_Teach) {
					inc_tone_slope +=
						TEACHMODE_DIST_INCR_SLOPE_LIM;
					dec_tone_slope +=
						TEACHMODE_DIST_DECR_SLOPE_LIM;
				} else {
					inc_tone_slope += DIST_INCR_SLOPE_LIM;
					dec_tone_slope += DIST_DECR_SLOPE_LIM;
				}

				if (dist_change > inc_tone_slope) {
					last_warn_freq += WARN_TONE_SPEED_HZ;
				}
				if (dist_change < dec_tone_slope) {
					last_warn_freq -= WARN_TONE_SPEED_HZ;
				}
			}

			bool try_zap = false;
			/* Clamp max frequency so we know sound controller
			 * plays the exact MAX frequency in order to
			 * also publish an event of MAX.
			 */
			if (last_warn_freq >= WARN_FREQ_MAX) {
				last_warn_freq = WARN_FREQ_MAX;
				try_zap = true;
			}

			/* We check if we're within valid frequency ranges. */
			if (last_warn_freq >= WARN_FREQ_INIT &&
			    last_warn_freq <= WARN_FREQ_MAX) {
				/** Check if buzzer is in warn event or was
				 *  interrupted playing other sounds in between.
				 *  We check if the variable is OFF, in which
				 *  case we know the buzzer is finished playing
				 *  what it had to play and we can continue with
				 *  warn zone event.
				 *  @note We might see multiple sound events being
				 *  published to event manager, since the atomic 
				 *  flag might not have been updated fast enough.
				 */
				if (atomic_get(&buzzer_off)) {
					/** Submit warn zone event to buzzer. 
					 * 
					 * @note This warn zone event 
					 * will play indefinitly
					 * unless one of the three happens and 
					 * it will turn off:
					 * 1. amc_correction doesn't update the 
					 *    warn frequency with a new value 
					 *    within 1 second  timeout (1 sec 
					 *    is the default value at least).
					 * 2. A new sound event has been submitted 
					 *    i.e. FIND_ME, or OFF, in which case 
					 *    we have to resubmit the 
					 *    SND_WARN event. This is currently 
					 *    implemented when we update the freq.
					 *    Where we submit the event if the
					 *    buzzer is IDLE.
					 * 3. It gets a frequency that is outside 
					 *    the freq range for instance below 
					 *    WARN_SOUND_INIT or higher than
					 *    WARN_SOUND_MAX, in which case 
					 *    it turns OFF.
					 */
					struct sound_event *snd_ev =
						new_sound_event();
					snd_ev->type = SND_WARN;
					EVENT_SUBMIT(snd_ev);
					LOG_INF("AMC notified buzzer to enter WARN");
				}

				/** Update buzzer frequency event. */
				struct sound_set_warn_freq_event *freq_ev =
					new_sound_set_warn_freq_event();
				freq_ev->freq = last_warn_freq;
				EVENT_SUBMIT(freq_ev);

				if (try_zap) {
					uint32_t delta_max = k_uptime_get_32() -
							     max_freq_timestamp;
					/* Check if we've played max freq for
					 * at least 5 seconds.
					 */
					if (delta_max > WARN_MIN_DURATION_MS) {
						correction_pause(
							Reason_WARNPAUSEREASON_ZAP,
							mean_dist);
						struct ep_status_event *ep_ev =
							new_ep_status_event();
						ep_ev->ep_status = EP_RELEASE;
						EVENT_SUBMIT(ep_ev);
						zap_eval_doing = 1;
						zap_timestamp =
							k_uptime_get_32();
						increment_zap_count();
						LOG_INF("AMC notified EP to zap!");

						/* Wait 5 more sec until
						 * next zap.
						 */
						max_freq_timestamp =
							k_uptime_get_32();
					}
				} else {
					max_freq_timestamp = k_uptime_get_32();
				}
			}
		} else {
			last_warn_dist = mean_dist + _LAST_DIST_ADD;
		}

		return;
	}
	return;
}

uint8_t get_correction_status(void)
{
	return correction_started + correction_warn_on;
}

void process_correction(Mode amc_mode, gnss_last_fix_struct_t *gnss,
			FenceStatus fs, amc_zone_t zone, int16_t mean_dist,
			int16_t dist_change)
{
	if (amc_mode == Mode_Teach || amc_mode == Mode_Fence) {
		if (zone == WARN_ZONE) {
			if (gnss->mode == GNSSMODE_MAX) {
				if (fs == FenceStatus_FenceStatus_Normal ||
				    fs == FenceStatus_MaybeOutOfFence) {
					/** @todo Fetch getActiveTime() from
					 *  movement controller.
					 */
					if (get_correction_status() > 0) {
						if (gnss_has_warn_fix()) {
							correction_start(
								mean_dist);
						}
					}
				}
			}
		}
	}

	/* Start main correction procedure. */
	correction(amc_mode, mean_dist, dist_change);

	/* Checks for pausing correction. */
	if ((amc_mode != Mode_Teach && amc_mode != Mode_Fence) ||
	    zone == NO_ZONE) {
		correction_pause(Reason_WARNSTOPREASON_MODE, mean_dist);
	} else if (fs == FenceStatus_Escaped) {
		correction_pause(Reason_WARNSTOPREASON_ESCAPED, mean_dist);
	} /** } @todo Why is gnss->age removed???? Where to fetch? Ano related?
	   *  else if (gnss-> > GNSS_1SEC) {
	   *	correction_pause(
	   *		Reason_WARNPAUSEREASON_MISSGPSDATA,
	   *		mean_dist); // Warning pause as result of missing GNSS.
	   */
	else if (!gnss_has_accepted_fix()) {
		/* Warning pause as result of bad position accuracy. */
		correction_pause(Reason_WARNPAUSEREASON_BADFIX, mean_dist);
	} else if (amc_mode == Mode_Fence) {
		if (mean_dist - last_warn_dist <= CORRECTION_PAUSE_DIST) {
			correction_pause(Reason_WARNPAUSEREASON_NODIST,
					 mean_dist);
		}
	} else if (amc_mode == Mode_Teach) {
		/* [LEGACY] see: https://youtrack.axbit.com/youtrack/issue/NOF-307
		 * 		if (acc_RawAmplitude(ACC_Y) > ACC_STOP_AMPLITUDE){			// This makes it easier for the animal to understand that it is in control and that it acctually is possible to turn off the warning
		 * 			correction_pause(Reason_WARNPAUSEREASON_ACC);			// Warning pause as result of that the accelerometer values shows sound reaction
		 * 		}
		 */
		if (mean_dist - last_warn_dist <=
		    TEACHMODE_CORRECTION_PAUSE_DIST) {
			correction_pause(Reason_WARNPAUSEREASON_NODIST,
					 mean_dist);
		}
		if (dist_change <= TEACHMODE_DIST_DECR_SLOPE_OFF_LIM) {
			/* Then animal has moved back, closer to fence. */
			correction_pause(Reason_WARNPAUSEREASON_MOVEBACK,
					 mean_dist);
		}
	}
	/* [LEGACY CODE] See http://youtrack.axbit.no/youtrack/issue/NOF-213. */
	if ((get_correction_status() < 2) && zone != WARN_ZONE) {
		/* Turn off warning only if it is already 
		 * paused when inside the pasture. 
		 */
		correction_pause(Reason_WARNSTOPREASON_INSIDE, mean_dist);
	}
}