#include "amc_test_common.h"
#include "amc_gnss.h"
#include <ztest.h>
#include "amc_states_cache.h"
#include "embedded.pb.h"
#include "gnss_controller_events.h"
#include "messaging_module_events.h"
#include "amc_cache.h"
#include "pwr_event.h"

K_SEM_DEFINE(fence_status_sem, 0, 1);
static FenceStatus current_fence_status = FenceStatus_FenceStatus_UNKNOWN;

K_SEM_DEFINE(collar_status_sem, 0, 1);
static CollarStatus current_collar_status = CollarStatus_CollarStatus_UNKNOWN;

K_SEM_DEFINE(collar_mode_sem, 0, 1);
static Mode current_collar_mode = Mode_Mode_UNKNOWN;

void test_fence_status(void)
{
	/* Check if we have valid pasture cached. */
	zassert_true(fnc_valid_def(), "");

	/* Init GNSS cache etc.. */
	amc_gnss_init();

	/* We expect NotStarted on boot. */
	FenceStatus expected_status = FenceStatus_NotStarted;
	zassert_equal(calc_fence_status(0, BEACON_STATUS_NOT_FOUND),
		      expected_status, "");
	zassert_equal(k_sem_take(&fence_status_sem, K_SECONDS(30)), 0, "");
	zassert_equal(current_fence_status, expected_status, "");

	/* Check force fence status functionality */
	for (int fence_status_idx = 0; fence_status_idx < 9; fence_status_idx++) {
		/* 9 fence statuses defined, starting with 0, (See Collar-protocol). */

		if ((fence_status_idx == FenceStatus_FenceStatus_UNKNOWN) ||
			(fence_status_idx == FenceStatus_NotStarted) ||
			(fence_status_idx == FenceStatus_FenceStatus_Invalid)) {
			if (fence_status_idx == FenceStatus_NotStarted) {
				/* Test EEPROM write failure */
				ztest_returns_value(eep_uint8_write, -1);
				zassert_not_equal(force_fence_status(FenceStatus_NotStarted), 0, 
							"Force fence status failed");
			}
			ztest_returns_value(eep_uint8_write, 0);
			zassert_equal(force_fence_status(fence_status_idx), 0, 
						"Force fence status failed");
			zassert_equal(k_sem_take(&fence_status_sem, K_SECONDS(30)), 0, 
						"Did not receive notification for fence status change");
			zassert_equal(current_fence_status, fence_status_idx, 
						"Force fence status failed to set correct status");
		} else {
			/* Should only force "UNKNOWN", "NotStarted" and "Invalid" */
			zassert_not_equal(force_fence_status(fence_status_idx), 0, 
						"Was able to force unavailable fence status");
		}
	}
	/* Set fence status to NotStarted for continued testing */
	if (get_fence_status() != FenceStatus_NotStarted) {
		ztest_returns_value(eep_uint8_write, 0);
		zassert_equal(force_fence_status(FenceStatus_NotStarted), 0, "");
	}

	/* NotStarted -> Normal */
	simulate_accepted_fix();
	zassert_equal(zone_set(PSM_ZONE), 0, "");

	ztest_returns_value(eep_uint8_write, 0);

	expected_status = FenceStatus_FenceStatus_Normal;
	zassert_equal(calc_fence_status(0, BEACON_STATUS_NOT_FOUND),
		      expected_status, "");

	k_sem_reset(&fence_status_sem);
	zassert_equal(k_sem_take(&fence_status_sem, K_SECONDS(30)), 0, "");
	zassert_equal(current_fence_status, expected_status, "");

	/* Normal -> Escaped */
	/* To get to escaped, we need to have zapped 3 times. Reset the zaps
	 * and zap 3 times.
	 */
	reset_zap_pain_cnt();

	for (int i = 0; i < 3; i++) {
		/* Mock total zaps and daily zaps. */
		ztest_returns_value(eep_uint16_write, 0);
		ztest_returns_value(eep_uint16_write, 0);

		increment_zap_count();
	}

	simulate_accepted_fix();

	/* Zone is irrelevant, the check is against how many zaps we've given
	 * regardless of position. 
	 */
	zassert_equal(zone_set(NO_ZONE), 0, "");

	ztest_returns_value(eep_uint8_write, 0);

	expected_status = FenceStatus_Escaped;
	zassert_equal(calc_fence_status(0, BEACON_STATUS_NOT_FOUND),
		      expected_status, "");
	k_sem_reset(&fence_status_sem);
	zassert_equal(k_sem_take(&fence_status_sem, K_SECONDS(30)), 0, "");
	zassert_equal(current_fence_status, expected_status, "");

	/* Escaped -> Normal */
	simulate_accepted_fix();
	zassert_equal(zone_set(CAUTION_ZONE), 0, "");

	ztest_returns_value(eep_uint8_write, 0);

	expected_status = FenceStatus_FenceStatus_Normal;
	zassert_equal(calc_fence_status(0, BEACON_STATUS_NOT_FOUND),
		      expected_status, "");
	zassert_equal(k_sem_take(&fence_status_sem, K_SECONDS(30)), 0, "");
	zassert_equal(current_fence_status, expected_status, "");

	/* Normal -> Beacon Contact normal */
	simulate_accepted_fix();
	zassert_equal(zone_set(CAUTION_ZONE), 0, "");

	ztest_returns_value(eep_uint8_write, 0);

	expected_status = FenceStatus_BeaconContactNormal;
	zassert_equal(calc_fence_status(0, BEACON_STATUS_REGION_NEAR),
		      expected_status, "");
	zassert_equal(k_sem_take(&fence_status_sem, K_SECONDS(30)), 0, "");
	zassert_equal(current_fence_status, expected_status, "");

	/* Beacon contact normal ->  Normal*/
	ztest_returns_value(eep_uint8_write, 0);

	expected_status = FenceStatus_FenceStatus_Normal;
	zassert_equal(calc_fence_status(0, BEACON_STATUS_NOT_FOUND),
		      expected_status, "");
	zassert_equal(k_sem_take(&fence_status_sem, K_SECONDS(30)), 0, "");
	zassert_equal(current_fence_status, expected_status, "");

	/* Normal -> Maybe out of fence */
	ztest_returns_value(eep_uint8_write, 0);

	/* We get to maybe out of fence if the delta is 900 or more.
	 * Set timestamp here, then wait 905 seconds. */
	uint32_t maybe_timestamp = k_uptime_get_32();

	k_sleep(K_SECONDS(905));

	expected_status = FenceStatus_MaybeOutOfFence;
	zassert_equal(calc_fence_status(maybe_timestamp,
					BEACON_STATUS_NOT_FOUND),
		      expected_status, "");
	zassert_equal(k_sem_take(&fence_status_sem, K_SECONDS(30)), 0, "");
	zassert_equal(current_fence_status, expected_status, "");

	/* Maybe out of fence -> Beacon Contact */
	ztest_returns_value(eep_uint8_write, 0);

	expected_status = FenceStatus_BeaconContact;

	/* Use same timestamp here, we know it's still as maybe out of fence. */
	zassert_equal(calc_fence_status(maybe_timestamp,
					BEACON_STATUS_REGION_NEAR),
		      expected_status, "");
	zassert_equal(k_sem_take(&fence_status_sem, K_SECONDS(30)), 0, "");
	zassert_equal(current_fence_status, expected_status, "");

	/* Beacon Contact -> NotStarted */
	ztest_returns_value(eep_uint8_write, 0);

	expected_status = FenceStatus_NotStarted;
	zassert_equal(calc_fence_status(0, BEACON_STATUS_NOT_FOUND),
		      expected_status, "");
	zassert_equal(k_sem_take(&fence_status_sem, K_SECONDS(30)), 0, "");
	zassert_equal(current_fence_status, expected_status, "");

	/* NotStarted -> Beacon Contact */
	ztest_returns_value(eep_uint8_write, 0);

	expected_status = FenceStatus_BeaconContact;
	zassert_equal(calc_fence_status(0, BEACON_STATUS_REGION_NEAR),
		      expected_status, "");
	zassert_equal(k_sem_take(&fence_status_sem, K_SECONDS(30)), 0, "");
	zassert_equal(current_fence_status, expected_status, "");

	/* Beacon Contact -> Unknown */
	/* We only enter this state if pasture is invalid. */
	pasture_t empty_pasture;
	memset(&empty_pasture, 0, sizeof(pasture_t));
	zassert_equal(set_pasture_cache((uint8_t *)&empty_pasture,
					sizeof(empty_pasture)),
		      0, "");

	ztest_returns_value(eep_uint8_write, 0);

	expected_status = FenceStatus_FenceStatus_UNKNOWN;
	zassert_equal(calc_fence_status(0, BEACON_STATUS_NOT_FOUND),
		      expected_status, "");
	zassert_equal(k_sem_take(&fence_status_sem, K_SECONDS(30)), 0, "");
	zassert_equal(current_fence_status, expected_status, "");

	/* Unknown -> Beacon Contact */
	/* We'll never reach NotStarted from unknown due to our
	 * pasture being invalid.
	 */
	ztest_returns_value(eep_uint8_write, 0);

	expected_status = FenceStatus_BeaconContact;
	zassert_equal(calc_fence_status(0, BEACON_STATUS_REGION_NEAR),
		      expected_status, "");
	zassert_equal(k_sem_take(&fence_status_sem, K_SECONDS(30)), 0, "");
	zassert_equal(current_fence_status, expected_status, "");
}

void test_collar_mode(void)
{
	/* We expect unknown when we boot. */
	Mode expected_mode = Mode_Mode_UNKNOWN;
	zassert_equal(k_sem_take(&collar_mode_sem, K_SECONDS(30)), 0, "");
	zassert_equal(current_collar_mode, expected_mode, "");

	/* Unknown -> Teach. */
	ztest_returns_value(eep_uint8_write, 0);
	expected_mode = Mode_Teach;

	zassert_equal(calc_mode(), expected_mode, "");
	zassert_equal(k_sem_take(&collar_mode_sem, K_SECONDS(30)), 0, "");
	zassert_equal(current_collar_mode, expected_mode, "");

	/* Teach -> Fence. */
	ztest_returns_value(eep_uint8_write, 0);
	/* To reach this, we need to reach the zap and warn count tresholds. 
	 * I.e ((teach_warn_cnt - teach_zap_cnt) >=
			    _TEACHMODE_WARN_CNT_LOWLIM)
	 * needs to be fulfilled. In other words, the animal must have
	 * managed to get warned 20 times without getting a zap.
	 */
	for (int i = 0; i < _TEACHMODE_WARN_CNT_LOWLIM + 1; i++) {
		/* Mock the eeprom calls. */
		ztest_returns_value(eep_uint32_write, 0);
		increment_warn_count();
	}
	ztest_returns_value(eep_uint8_write, 0);
	expected_mode = Mode_Fence;

	zassert_equal(calc_mode(), expected_mode, "");
	zassert_equal(k_sem_take(&collar_mode_sem, K_SECONDS(30)), 0, "");
	zassert_equal(current_collar_mode, expected_mode, "");
}

void test_collar_status(void)
{
	// /* We expect unknown when we boot. */
	// CollarStatus expected_status = CollarStatus_CollarStatus_UNKNOWN;
	// zassert_equal(k_sem_take(&collar_status_sem, K_SECONDS(30)), 0, "");
	// zassert_equal(current_collar_status, expected_status, "");

	/* Unknown -> normal. */
	//ztest_returns_value(eep_uint8_write, 0);
	update_movement_state(STATE_NORMAL);

	CollarStatus expected_status = CollarStatus_CollarStatus_Normal;
	zassert_equal(calc_collar_status(), expected_status, "");
	zassert_equal(k_sem_take(&collar_status_sem, K_SECONDS(30)), 0, "");
	zassert_equal(current_collar_status, expected_status, "");

	/* Normal -> sleep. */
	ztest_returns_value(eep_uint8_write, 0);
	update_movement_state(STATE_SLEEP);

	expected_status = CollarStatus_Sleep;
	zassert_equal(calc_collar_status(), expected_status, "");
	zassert_equal(k_sem_take(&collar_status_sem, K_SECONDS(30)), 0, "");
	zassert_equal(current_collar_status, expected_status, "");

	/* Sleep -> OffAnimal. */
	ztest_returns_value(eep_uint8_write, 0);
	update_movement_state(STATE_INACTIVE);

	expected_status = CollarStatus_OffAnimal;
	zassert_equal(calc_collar_status(), expected_status, "");
	zassert_equal(k_sem_take(&collar_status_sem, K_SECONDS(30)), 0, "");
	zassert_equal(current_collar_status, expected_status, "");

	/* OffAnimal -> normal. */
	ztest_returns_value(eep_uint8_write, 0);
	update_movement_state(STATE_NORMAL);

	expected_status = CollarStatus_CollarStatus_Normal;
	zassert_equal(calc_collar_status(), expected_status, "");
	zassert_equal(k_sem_take(&collar_status_sem, K_SECONDS(30)), 0, "");
	zassert_equal(current_collar_status, expected_status, "");

	/* Normal -> STATE_INACTIVE (Normal) (do nothing). */
	update_movement_state(STATE_INACTIVE);

	expected_status = CollarStatus_CollarStatus_Normal;
	zassert_equal(calc_collar_status(), expected_status, "");
	/* No event is triggered, since we're in the same state. */

	/* Normal -> Power OFF. */
	ztest_returns_value(eep_uint8_write, 0);
	update_movement_state(STATE_NORMAL);
	update_power_state(PWR_CRITICAL);

	expected_status = CollarStatus_PowerOff;
	zassert_equal(calc_collar_status(), expected_status, "");
	zassert_equal(k_sem_take(&collar_status_sem, K_SECONDS(30)), 0, "");
	zassert_equal(current_collar_status, expected_status, "");

	/* Power OFF -> Sleep. */
	ztest_returns_value(eep_uint8_write, 0);
	update_movement_state(STATE_SLEEP);
	update_power_state(PWR_LOW);

	expected_status = CollarStatus_Sleep;
	zassert_equal(calc_collar_status(), expected_status, "");
	zassert_equal(k_sem_take(&collar_status_sem, K_SECONDS(30)), 0, "");
	zassert_equal(current_collar_status, expected_status, "");
}

static bool event_handler(const struct event_header *eh)
{
	if (is_update_collar_status(eh)) {
		struct update_collar_status *ev = cast_update_collar_status(eh);
		current_collar_status = ev->collar_status;
		k_sem_give(&collar_status_sem);
		return false;
	}
	if (is_update_fence_status(eh)) {
		struct update_fence_status *ev = cast_update_fence_status(eh);
		current_fence_status = ev->fence_status;
		k_sem_give(&fence_status_sem);
		return false;
	}
	if (is_update_collar_mode(eh)) {
		struct update_collar_mode *ev = cast_update_collar_mode(eh);
		current_collar_mode = ev->collar_mode;
		k_sem_give(&collar_mode_sem);
		return false;
	}
	return false;
}

EVENT_LISTENER(test_collar_fence_handler, event_handler);
EVENT_SUBSCRIBE(test_collar_fence_handler, update_collar_status);
EVENT_SUBSCRIBE(test_collar_fence_handler, update_fence_status);
EVENT_SUBSCRIBE(test_collar_fence_handler, update_collar_mode);