/*
 * Copyright (c) 2021 Nofence AS
 */

#include <zephyr.h>
#include <logging/log.h>

#include "amc_cache.h"
#include "amc_dist.h"
#include "amc_zone.h"
#include "amc_gnss.h"
#include "amc_states_cache.h"
#include "amc_correction.h"
#include "amc_const.h"
#include "nf_fifo.h"
#include "amc_handler.h"
#include "amc_events.h"
#include "sound_event.h"
#include "request_events.h"
#include "pasture_structure.h"
#include "event_manager.h"
#include "error_event.h"
#include "messaging_module_events.h"
#include "gnss_controller_events.h"
#include "ble_beacon_event.h"
#include "ble_ctrl_event.h"
#include "movement_events.h"
#include "storage.h"
#include "stg_config.h"

#define MODULE amc
//LOG_MODULE_REGISTER(MODULE, CONFIG_AMC_LOG_LEVEL);
LOG_MODULE_REGISTER(MODULE, 0);

/* Thread stack area that we use for the calculation process. We add a work
 * item here when we have data available from GNSS. This thread can then use the 
 * calculation function algorithm to determine what it needs to do in regards to 
 * sound and zap events that this calculation function can submit to event 
 * handler. */
K_THREAD_STACK_DEFINE(amc_calculation_thread_area, CONFIG_AMC_CALCULATION_SIZE);

/* AMC work queue */
static struct k_work_q amc_work_q;

/* AMC work items used to call AMC's main functionalities. */
static struct k_work handle_new_gnss_work;
static struct k_work handle_states_work;
static struct k_work handle_corrections_work;
static struct k_work handle_new_fence_work;

/* Distance arrays. */
static int16_t dist_array[FIFO_ELEMENTS];
static int16_t dist_avg_array[FIFO_AVG_DISTANCE_ELEMENTS];

/* Array to discover unstable position accuracy. */
static int16_t acc_array[FIFO_ELEMENTS];

/* Array used to discover abnormal height changes that might happen
 * if gps signals gets reflected. */
static int16_t height_avg_array[FIFO_ELEMENTS];

/* Static variables used in AMC logic. */
static int16_t dist_change;
static int16_t mean_dist = INT16_MIN;
static int16_t instant_dist = INT16_MIN;
static uint8_t fifo_dist_elem_count = 0;
static uint8_t fifo_avg_dist_elem_count = 0;

/* Cached variable to check which state the buzzer is in. 
 * false = SND_OFF
 * true  = SND_WARN or SND_MAX */
atomic_t buzzer_state = ATOMIC_INIT(false);

/* To make sure we're in the sound max before we submit EP release. */
atomic_t sound_max_atomic = ATOMIC_INIT(false);

/* Beacon status used in calculating the fence status. */
atomic_t current_beacon_status = ATOMIC_INIT(BEACON_STATUS_NOT_FOUND);

static uint32_t maybe_out_of_fence_timestamp = 0;

static bool m_fence_update_pending = false;
static uint32_t m_new_fence_version;
/**
 * @brief Updates the current pasture from storage (if a valid fence).
 * 
 * @return Zero if successfull, otherwise negative error code.
 */
static inline int update_pasture_from_stg(void);

/**
 * @brief Resets the distance elements FIFO for GNSS timeout.
 * 
 * @return Returns 0 indicating successful execution.
 */
int gnss_timeout_reset_fifo();

/**
 * @brief Work handler function that handles and deploy a new fence when 
 * available from storage.
 * 
 * @param item Pointer to the work item.
 */
void handle_new_fence_fn(struct k_work *item);

/**
 * @brief Work handler function that re-calculates the fence status, collar 
 * status and collar mode.
 */
void handle_states_fn();

/**
 * @brief Work handler function that perform corrections based on the mode 
 * during the function call instance due to get-modes within the function. Does 
 * not perform distance calculations nor re-calculate the modes. Gets cached 
 * distances and FIFO's and then fetches the fence status and collar status, and 
 * ultimately performs the corrections.
 */
void handle_corrections_fn();

/**
 * @brief Work handler function for processing new gnss data that have just been 
 * received and stored.
 * 
 * @param item Pointer to the work item.
 */
void handle_gnss_data_fn(struct k_work *item);

/**
 * @brief Main event handler function. 
 * 
 * @param[in] eh Event_header for the if-chain to use to recognize which event 
 * triggered.
 * @return true to consume the event, otherwise false.
 */
static bool event_handler(const struct event_header *eh);

static inline int update_pasture_from_stg(void)
{
	int err;
	/* Add check for fence status, in case collar has rebooted after invalidation */
	if (get_fence_status() == FenceStatus_TurnedOffByBLE) {
		LOG_WRN("Fence is turned off by BLE");
		/* Submit event to reset the cached fence version to 0. */
		struct update_fence_version *ver = new_update_fence_version();
		ver->fence_version = 0;
		ver->total_fences = 0;
		EVENT_SUBMIT(ver);
		return 0;
	}

	err = stg_read_pasture_data(set_pasture_cache);
	if (err == -ENODATA) {
		LOG_WRN("No pasture found on external flash. (%d)", err);
		nf_app_warning(ERR_AMC, err, NULL, 0);
		/* Submit event that we have now begun to use the new fence. */
		struct update_fence_version *ver = new_update_fence_version();
		ver->fence_version = 0;
		ver->total_fences = 0;
		EVENT_SUBMIT(ver);
		return 0;
	} else if (err) {
		LOG_ERR("Could not update pasture cache in AMC. (%d)", err);
		nf_app_fatal(ERR_AMC, err, NULL, 0);

		/* Set pasture/fence to invalid */
		force_fence_status(FenceStatus_FenceStatus_Invalid);
		return err;
	}

	/* Take pasture sem, since we need to access the version to send to messaging module. */
	err = k_sem_take(&fence_data_sem, K_SECONDS(CONFIG_FENCE_CACHE_TIMEOUT_SEC));
	do {
		if (err) {
			LOG_ERR("Error taking pasture semaphore for version check (%d)", err);
			nf_app_error(ERR_AMC, err, NULL, 0);
			break;
		}

		/* Verify it has been cached correctly. */
		pasture_t *pasture = NULL;
		get_pasture_cache(&pasture);
		if (pasture == NULL) {
			LOG_ERR("Pasture was not cached correctly. (%d)", -ENODATA);
			nf_app_error(ERR_AMC, -ENODATA, NULL, 0);
			err = -ENODATA;
			break;
		}

		/* New pasture installed, force a new gnss fix. */
		restart_force_gnss_to_fix();

		if (m_fence_update_pending) {
			m_fence_update_pending = false;

			/* Success setting the pasture. set to teach mode if not keepmode active. */
			uint8_t keep_mode;
			err = stg_config_u8_read(STG_U8_KEEP_MODE, &keep_mode);
			if (err != 0) {
				LOG_ERR("Error reading keep mode from storage. (%d)", err);
				nf_app_warning(ERR_AMC, err, NULL, 0);

				keep_mode = 0; //Defaults to 0 in case of read failure
			}
			/* New pasture installed, set teach mode. */
			if (!keep_mode) {
				force_teach_mode();
			}

			/* Update fence status, this also notify listeners */
			if (pasture->m.ul_fence_def_version != m_new_fence_version) {
				force_fence_status(FenceStatus_FenceStatus_Invalid);
			} else {
				err = force_fence_status(FenceStatus_NotStarted);
				if (err != 0) {
					break;
				}
			}
		}

		/* Submit event that we have now begun to use the new fence. */
		struct update_fence_version *ver = new_update_fence_version();
		LOG_INF("UPDATING FENCE VERSION TO = %u", pasture->m.ul_fence_def_version);
		ver->fence_version = pasture->m.ul_fence_def_version;
		ver->total_fences = pasture->m.ul_total_fences;
		EVENT_SUBMIT(ver);

		LOG_INF("Pasture loaded:FenceVersion=%d,FenceStatus=%d",
			pasture->m.ul_fence_def_version, get_fence_status());

		k_sem_give(&fence_data_sem);
		return 0;
	} while (0);
	LOG_WRN("Failed to update pasture!");
	/* Update fence status to unknown in case of failure */
	force_fence_status(FenceStatus_FenceStatus_Invalid);
	k_sem_give(&fence_data_sem);
	return err;
}

void handle_new_fence_fn(struct k_work *item)
{
	int err;
	m_fence_update_pending = true;
	zone_set(NO_ZONE);

	if (get_fence_status() == FenceStatus_TurnedOffByBLE) {
		err = force_fence_status(FenceStatus_FenceStatus_UNKNOWN);
		if (err != 0) {
			LOG_ERR("Set fence status to UNKNOWN failed %d", err);
		}
	}

	err = update_pasture_from_stg();
	if (err != 0) {
		LOG_WRN("Fence update request denied, error:%d", err);
	}
	k_work_submit_to_queue(&amc_work_q, &handle_states_work);
	return;
}

void handle_states_fn()
{
	/* Get states, and recalculate if we have new state changes. */
	Mode amc_mode = calc_mode();
	FenceStatus fence_status = get_fence_status();
	amc_zone_t cur_zone = zone_get();

	if (cur_zone != WARN_ZONE || buzzer_state || fence_status == FenceStatus_NotStarted ||
	    fence_status == FenceStatus_Escaped) {
		/** @todo Based on uint32, we can only stay uptime for 1.6 months, 
		 * (1193) hours before it wraps around. Issue? We have k_uptime_get_32 
		 * other places as well. */
		maybe_out_of_fence_timestamp = k_uptime_get_32();
	}

	FenceStatus new_fence_status =
		calc_fence_status(maybe_out_of_fence_timestamp, atomic_get(&current_beacon_status));

	CollarStatus new_collar_status = calc_collar_status();

	set_sensor_modes(amc_mode, new_fence_status, new_collar_status, cur_zone);

	LOG_DBG("AMC states:CollarMode=%d,CollarStatus=%d,Zone=%d,FenceStatus=%d", get_mode(),
		get_collar_status(), zone_get(), get_fence_status());
}

void handle_corrections_fn()
{
	/* Fetch cached gnss data. */
	LOG_INF("  handle_corrections_fn");
	gnss_t *gnss = NULL;
	int err = get_gnss_cache(&gnss);
	if ((err != 0) && (err != -ETIMEDOUT)) {
		LOG_ERR("Could not fetch GNSS cache %i", err);
		return;
	}

	Mode collar_mode = get_mode();
	FenceStatus fence_status = get_fence_status();
	amc_zone_t current_zone = zone_get();

	process_correction(collar_mode, &gnss->lastfix, fence_status, current_zone, mean_dist,
			   dist_change);
}

void handle_gnss_data_fn(struct k_work *item)
{
	if (m_fence_update_pending) {
		LOG_DBG("AMC GNSS data not processed due to pending fence update");
		goto cleanup;
	}

	/* Take fence semaphore since we're going to use the cached area. */
	int err = k_sem_take(&fence_data_sem, K_SECONDS(CONFIG_FENCE_CACHE_TIMEOUT_SEC));
	if (err) {
		LOG_ERR("Error waiting for fence data semaphore to release. (%d)", err);
		nf_app_error(ERR_AMC, err, NULL, 0);
		goto cleanup;
	}

	/* Fetch cached fence and size. */
	pasture_t *pasture = NULL;
	err = get_pasture_cache(&pasture);
	if (err || pasture == NULL) {
		goto cleanup;
	}

	/* Fetch new, cached gnss data. */
	gnss_t *gnss = NULL;
	bool gnss_timeout = false;
	err = get_gnss_cache(&gnss);
	if (err == -ETIMEDOUT) {
		LOG_WRN("GNSS data invalid, timed out");
		gnss_timeout = true;
	} else if (err != 0) {
		goto cleanup;
	}

	LOG_DBG("\n\n--== START ==--");
	LOG_DBG("  GNSS data: %d, %d, %d, %d, %d", gnss->latest.lon, gnss->latest.lat,
		gnss->latest.pvt_flags, gnss->latest.h_acc_dm, gnss->latest.num_sv);

	/* Set local variables used in AMC logic. */
	int16_t height_delta = INT16_MAX;
	int16_t acc_delta = INT16_MAX;
	int16_t dist_avg_change = 0;
	uint8_t dist_inc_count = 0;

	/* Validate position */
	err = gnss_update(gnss);
	if (err) {
		goto cleanup;
	}

	/* Fetch x and y position based on gnss data */
	int16_t pos_x = 0, pos_y = 0;
	err = gnss_calc_xy(gnss, &pos_x, &pos_y, pasture->m.l_origin_lon, pasture->m.l_origin_lat,
			   pasture->m.us_k_lon, pasture->m.us_k_lat);
	bool overflow_xy = (err == -EOVERFLOW);

	struct xy_location *loc = new_xy_location();
	loc->x = pos_x;
	loc->y = pos_y;
	EVENT_SUBMIT(loc);

	/* If any fence (pasture?) is valid and we have fix. */
	if (gnss_has_fix() && fnc_valid_fence() && !overflow_xy && !gnss_timeout) {
		/* Calculate distance to closest polygon. */
		uint8_t fence_index = 0;
		uint8_t vertex_index = 0;
		instant_dist = fnc_calc_dist(pos_x, pos_y, &fence_index, &vertex_index);
		LOG_INF("  Calculated distance: %d", instant_dist);

		/* Reset dist_change since we acquired a new distance. */
		dist_change = 0;

		/* if ((GPS_GetMode() == GPSMODE_MAX) 
		*  && TestBits(m_u16_PosAccuracy, GPSFIXOK_MASK)) { ??????
	 	*/
		/** @todo We should do something about that GPS_GetMode thing 
		  * here. Either add check to has_accepted_fix, or explicit 
		  * call to gnss_get_mode */
		if (gnss_has_accepted_fix()) {
			LOG_INF("  Has accepted fix!");

			/* Accepted position. Fill FIFOs. */
			fifo_put(gnss->lastfix.h_acc_dm, acc_array, FIFO_ELEMENTS);
			fifo_put(gnss->lastfix.height, height_avg_array, FIFO_ELEMENTS);
			fifo_put(instant_dist, dist_array, FIFO_ELEMENTS);

			/* If we have filled the distance FIFO, calculate
			 * the average and store that value into
			 * another avg_dist FIFO.
			 */
			if (++fifo_dist_elem_count >= FIFO_ELEMENTS) {
				fifo_dist_elem_count = 0;
				fifo_put(fifo_avg(dist_array, FIFO_ELEMENTS), dist_avg_array,
					 FIFO_AVG_DISTANCE_ELEMENTS);

				if (++fifo_avg_dist_elem_count >= FIFO_AVG_DISTANCE_ELEMENTS) {
					fifo_avg_dist_elem_count = FIFO_AVG_DISTANCE_ELEMENTS;
				}
			}
		} else {
			LOG_INF("  Does not have accepted fix!");
			fifo_dist_elem_count = 0;
			fifo_avg_dist_elem_count = 0;
		}

		if (fifo_avg_dist_elem_count > 0) {
			/* Fill avg/mean/delta fifos as we have collected
			 * valid data over a short period. */
			mean_dist = fifo_avg(dist_array, FIFO_ELEMENTS);
			dist_change = fifo_slope(dist_array, FIFO_ELEMENTS);
			dist_inc_count = fifo_inc_cnt(dist_array, FIFO_ELEMENTS);
			acc_delta = fifo_delta(acc_array, FIFO_ELEMENTS);
			height_delta = fifo_delta(height_avg_array, FIFO_ELEMENTS);
			if (fifo_avg_dist_elem_count >= FIFO_AVG_DISTANCE_ELEMENTS) {
				dist_avg_change =
					fifo_slope(dist_avg_array, FIFO_AVG_DISTANCE_ELEMENTS);
			}
		}
		LOG_INF("  mean_dist: %d, dist_change: %d, dist_inc_count: %d, acc_delta: %d, height_delta: %d",
			mean_dist, dist_change, dist_inc_count, acc_delta, height_delta);

		int16_t dist_incr_slope_lim = 0;
		uint8_t dist_incr_count = 0;

		/* Set slopes and count based on mode. */
		if (get_mode() == Mode_Teach) {
			dist_incr_slope_lim = TEACHMODE_DIST_INCR_SLOPE_LIM;
			dist_incr_count = TEACHMODE_DIST_INCR_COUNT;
		} else {
			dist_incr_slope_lim = DIST_INCR_SLOPE_LIM;
			dist_incr_count = DIST_INCR_COUNT;
		}

		/* Evaluate and possibly change AMC Zone */
		amc_zone_t cur_zone = zone_get();
		amc_zone_t old_zone = cur_zone;
		err = zone_update(instant_dist, gnss, &cur_zone);
		if (err != 0) {
			fifo_dist_elem_count = 0;
			fifo_avg_dist_elem_count = 0;
		}

		/* If the zone changes from NoZone, PSM or Caution to PreWarn or Warn, start beacon 
		* scanning, to reduce the possiblity that an animal just entering the barn 
		* (near the pasture border) get sound/pulse due to beacon scanning interval */
		if ((old_zone != PREWARN_ZONE) && (old_zone != WARN_ZONE) &&
		    ((cur_zone == PREWARN_ZONE) || (cur_zone == WARN_ZONE))) {
			struct ble_ctrl_event *event = new_ble_ctrl_event();
			event->cmd = BLE_CTRL_SCAN_START;
			EVENT_SUBMIT(event);
		}

		/* Set final accuracy flags based on previous calculations. */
		err = gnss_update_dist_flags(dist_avg_change, dist_change, dist_incr_slope_lim,
					     dist_inc_count, dist_incr_count, height_delta,
					     acc_delta, mean_dist, gnss->lastfix.h_acc_dm);
		if (err) {
			goto cleanup;
		}

		k_work_submit_to_queue(&amc_work_q, &handle_states_work);
		k_work_submit_to_queue(&amc_work_q, &handle_corrections_work);
	} else if (gnss_timeout) {
		/* GNSS timed out, set zone to NO_ZONE and schedule correction
		 * work in order to stop correction if already running */
		fifo_dist_elem_count = 0;
		fifo_avg_dist_elem_count = 0;
		zone_set(NO_ZONE);

		k_work_submit_to_queue(&amc_work_q, &handle_states_work);
		k_work_submit_to_queue(&amc_work_q, &handle_corrections_work);
	} else {
		fifo_dist_elem_count = 0;
		fifo_avg_dist_elem_count = 0;
		zone_set(NO_ZONE);
	}

cleanup:
	/* Calculation finished, give semaphore so we can swap memory region
	 * on next GNSS request. Also notifying we're not using fence data area. */
	k_sem_give(&fence_data_sem);
	LOG_DBG("--== END ==--\n\n");
}

int gnss_timeout_reset_fifo()
{
	fifo_dist_elem_count = 0;
	fifo_avg_dist_elem_count = 0;
	return 0;
}

int amc_module_init(void)
{
	/* Init work item and start and init calculation work queue thread and 
	 * item. */
	k_work_queue_init(&amc_work_q);
	k_work_queue_start(&amc_work_q, amc_calculation_thread_area,
			   K_THREAD_STACK_SIZEOF(amc_calculation_thread_area),
			   CONFIG_AMC_CALCULATION_PRIORITY, NULL);

	k_work_init(&handle_new_gnss_work, handle_gnss_data_fn);
	k_work_init(&handle_new_fence_work, handle_new_fence_fn);
	k_work_init(&handle_corrections_work, handle_corrections_fn);
	k_work_init(&handle_states_work, handle_states_fn);

	//int err = gnss_init(gnss_timeout_reset_fifo);
	//if (err) {
	//	return err;
	//}

	/* Checks and inits the mode we're in as well as caching variables. */
	init_states_and_variables();

	/* Fetch the fence from external flash and update fence cache. */
	return update_pasture_from_stg();
}

static bool event_handler(const struct event_header *eh)
{
	if (is_new_fence_available(eh)) {
		struct new_fence_available *ev = cast_new_fence_available(eh);
		m_new_fence_version = ev->new_fence_version;
		k_work_submit_to_queue(&amc_work_q, &handle_new_fence_work);
		return false;
	}
	if (is_gnss_data(eh)) {
		struct gnss_data *event = cast_gnss_data(eh);

		int err = set_gnss_cache(&event->gnss_data, event->timed_out);
		if (err) {
			LOG_ERR("Could not set gnss cahce. (%d)", err);
			nf_app_error(ERR_AMC, err, NULL, 0);
			return false;
		}

		/* No need to handle states/correction here, this is done
		 * within handle_new_gnss_work. */
		k_work_submit_to_queue(&amc_work_q, &handle_new_gnss_work);
		return false;
	}
	if (is_ble_beacon_event(eh)) {
		struct ble_beacon_event *event = cast_ble_beacon_event(eh);
		atomic_set(&current_beacon_status, event->status);

		/* Process the states, recalculate. */
		k_work_submit_to_queue(&amc_work_q, &handle_states_work);
		return false;
	}
	if (is_pwr_status_event(eh)) {
		struct pwr_status_event *event = cast_pwr_status_event(eh);
		update_power_state(event->pwr_state);

		/* Process the states, recalculate. */
		k_work_submit_to_queue(&amc_work_q, &handle_states_work);
		return false;
	}
	if (is_sound_status_event(eh)) {
		const struct sound_status_event *event = cast_sound_status_event(eh);
		if (event->status == SND_STATUS_PLAYING_MAX) {
			atomic_set(&buzzer_state, true);
			atomic_set(&sound_max_atomic, true);
		} else if (event->status == SND_STATUS_PLAYING_WARN) {
			atomic_set(&buzzer_state, true);
			atomic_set(&sound_max_atomic, false);
		} else {
			atomic_set(&buzzer_state, false);
			atomic_set(&sound_max_atomic, false);
		}
		return false;
	}
	if (is_movement_out_event(eh)) {
		const struct movement_out_event *ev = cast_movement_out_event(eh);
		update_movement_state(ev->state);
		k_work_submit_to_queue(&amc_work_q, &handle_states_work);
		return false;
	}
	if (is_turn_off_fence_event(eh)) {
		int err = force_fence_status(FenceStatus_TurnedOffByBLE);
		if (err != 0) {
			LOG_ERR("Turn off fence over BLE failed %d", err);
		}
		/* Update fence version to set valid fence advertised to false */
		struct update_fence_version *ver = new_update_fence_version();
		ver->fence_version = UINT32_MAX;
		ver->total_fences = 0;
		EVENT_SUBMIT(ver);

		return false;
	}
	if (is_gnss_mode_changed_event(eh)) {
		const struct gnss_mode_changed_event *ev = cast_gnss_mode_changed_event(eh);
		gnss_set_mode(ev->mode);
	}
	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);
	return false;
}

EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, gnss_data);
EVENT_SUBSCRIBE(MODULE, new_fence_available);
EVENT_SUBSCRIBE(MODULE, ble_beacon_event);
EVENT_SUBSCRIBE(MODULE, sound_status_event);
EVENT_SUBSCRIBE(MODULE, movement_out_event);
EVENT_SUBSCRIBE(MODULE, turn_off_fence_event);
EVENT_SUBSCRIBE(MODULE, gnss_mode_changed_event);
