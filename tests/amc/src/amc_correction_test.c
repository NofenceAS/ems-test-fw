/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "amc_test_common.h"
#include "amc_gnss.h"
#include "amc_states_cache.h"
#include "embedded.pb.h"
#include "amc_cache.h"
#include "movement_events.h"
#include "amc_correction.h"
#include "amc_const.h"
#include "sound_event.h"
#include "messaging_module_events.h"
#include "ep_event.h"

K_SEM_DEFINE(warning_start_sem, 0, 1);
K_SEM_DEFINE(warning_pause_sem, 0, 1);
K_SEM_DEFINE(warning_stop_sem, 0, 1);
K_SEM_DEFINE(animal_warning_sem, 0, 1);

K_SEM_DEFINE(sound_event_sem, 0, 1);
static uint8_t m_sound_type = SND_OFF;

K_SEM_DEFINE(warning_freq_sem, 0, 1);
static uint32_t m_warning_freq = 0;

static gnss_t gnss_data_fix = { 
    .latest = { .pvt_flags = 1 },
    .fix_ok = true,
    .lastfix = { .pvt_flags = 1, .mode = GNSSMODE_MAX, .updated_at = 0 },
    .has_lastfix = true
};

static uint32_t convert_to_legacy_freq(uint32_t freq);

void test_correction_mode(void)
{
    /* 
     * Test that AMC correction is started/not started based on the correct
     * collar modes accordingly. AMC correction shall only start for Collar
     * mode Mode_Fence and Mode_Teach.
     */

	Mode collar_mode = Mode_Mode_UNKNOWN;
	FenceStatus fence_status = FenceStatus_FenceStatus_Normal;
	amc_zone_t current_zone = WARN_ZONE;
    uint16_t mean_dist = 0;
    uint16_t dist_change = 0;

    k_sleep(K_SECONDS(CORRECTION_PAUSE_MIN_TIME + 1)); 

    /* Enable GNSS and simulate a warning fix */
    amc_gnss_init();
    simulate_warn_fix();

    /*
     * Verify that all collar modes that should not start AMC correction don't 
     * start AMC correction.
     */
    for (int i = 0; i < (Mode_Trace + 1); i++) {
        /* Mode_Trace is the last collar mode in the definition, see 
        (collar-protocol/base.proto/Mode protobuf definition).*/
        collar_mode = i;
        if ((collar_mode == Mode_Teach) || (collar_mode == Mode_Fence)) {
            continue;
        }

        gnss_data_fix.lastfix.updated_at = k_uptime_get_32();
        k_sem_reset(&warning_start_sem);

        /* Attempt to start correction */
        process_correction(collar_mode, &gnss_data_fix.lastfix, fence_status, 
            current_zone, mean_dist, dist_change);

        /* Wait for warning start event to timeout- no start event received */
        zassert_not_equal(k_sem_take(&warning_start_sem, K_SECONDS(5)), 0, "");

        /* Check that correction status is 0, e.g. correction not started */
        zassert_equal(get_correction_status(), 0, "");
    }

    /*
     * Verify that all collar modes that should start AMC correction does.
     */
    for (int i = 0; i < (Mode_Trace + 1); i++) {
        /* Mode_Trace is the last collar mode in the definition, see 
        (collar-protocol/base.proto/Mode protobuf definition).*/
        collar_mode = i;
        if ((collar_mode != Mode_Teach) && (collar_mode != Mode_Fence)) {
            continue;
        }

        gnss_data_fix.lastfix.updated_at = k_uptime_get_32();
        k_sem_reset(&warning_start_sem);

        /* Attempt to start correction */
        ztest_returns_value(get_active_delta, STATE_NORMAL);
        ztest_returns_value(stg_config_u32_write, 0);
        process_correction(collar_mode, &gnss_data_fix.lastfix, fence_status, 
            current_zone, mean_dist, dist_change);

        /* Wait for warning start event */
        zassert_equal(k_sem_take(&warning_start_sem, K_SECONDS(5)), 0, "");

        /* Check that correction status is 2, e.g. started and warning ON  */
        zassert_equal(get_correction_status(), 2, "");

        /* Stop correction by updating correction with Mode_Unknown */
        collar_mode = Mode_Mode_UNKNOWN;
        gnss_data_fix.lastfix.updated_at = k_uptime_get_32();
        k_sem_reset(&warning_stop_sem);

        /* Update correction */
        process_correction(collar_mode, &gnss_data_fix.lastfix, fence_status, 
            current_zone, mean_dist, dist_change);

        /* Wait for correction ended event */
        zassert_equal(k_sem_take(&warning_stop_sem, K_SECONDS(5)), 0, "");

        /* Check that correction status is 0, e.g. correction not started */
        zassert_equal(get_correction_status(), 0, "");

        /* Wait for correction pause timeout */
        k_sleep(K_SECONDS(CORRECTION_PAUSE_MIN_TIME + 1));
    }
}

void test_correction_fence_status(void)
{
    /* 
     * Test that AMC correction is started/not started based on the correct 
     * fence statuses accordingly. AMC correction shall only started for 
     * FenceStatus_Normal and FenceStatus_MaybeOutOfFence.
     */

	Mode collar_mode = Mode_Teach;
	FenceStatus fence_status = FenceStatus_FenceStatus_UNKNOWN;
	amc_zone_t current_zone = WARN_ZONE;
    uint16_t mean_dist = 0;
    uint16_t dist_change = 0;

    k_sleep(K_SECONDS(CORRECTION_PAUSE_MIN_TIME + 1)); 

    /* Enable GNSS and simulate a warning fix */
    amc_gnss_init();
    simulate_warn_fix();

    /*
     * Verify that all fence statuses that should not start AMC correction does
     * not start AMC correction.
     */
    for (int i = 0; i < (FenceStatus_TurnedOffByBLE + 1); i++) {
        /* FenceStatus_TurnedOffByBLE is the last fence status in the 
        definition, see (collar-protocol/FenceStatus protobuf definition). */
        fence_status = i;
        if ((fence_status == FenceStatus_FenceStatus_Normal) || 
            (fence_status == FenceStatus_MaybeOutOfFence)) {
            continue;
        }
        
        gnss_data_fix.lastfix.updated_at = k_uptime_get_32();
        k_sem_reset(&warning_start_sem);

        /* Attempt to start correction */
        process_correction(collar_mode, &gnss_data_fix.lastfix, fence_status, 
            current_zone, mean_dist, dist_change);

        /* Wait for warning start event to timeout- no start event received */
        zassert_not_equal(k_sem_take(&warning_start_sem, K_SECONDS(5)), 0, "");

        /* Check that correction status is 0, e.g. correction not started */
        zassert_equal(get_correction_status(), 0, "");
    }

    /*
     * Verify that all fence statuses that should start AMC correction does.
     */
    for (int i = 0; i < (FenceStatus_TurnedOffByBLE + 1); i++) {
        /* FenceStatus_TurnedOffByBLE is the last fence status in the 
        definition, see (collar-protocol/FenceStatus protobuf definition). */
        collar_mode = Mode_Teach;
        fence_status = i;
        if ((fence_status != FenceStatus_FenceStatus_Normal) && 
            (fence_status != FenceStatus_MaybeOutOfFence)) {
            continue;
        }
        
        gnss_data_fix.lastfix.updated_at = k_uptime_get_32();
        k_sem_reset(&warning_start_sem);

        /* Attempt to start correction */
        ztest_returns_value(get_active_delta, STATE_NORMAL);
        ztest_returns_value(stg_config_u32_write, 0);
        process_correction(collar_mode, &gnss_data_fix.lastfix, fence_status, 
            current_zone, mean_dist, dist_change);

        /* Wait for warning start event */
        zassert_equal(k_sem_take(&warning_start_sem, K_SECONDS(5)), 0, "");

        /* Check that correction status is 2, e.g. started and warning ON  */
        zassert_equal(get_correction_status(), 2, "");

        /* Stop correction by updating correction with Mode_Unknown */
        collar_mode = Mode_Mode_UNKNOWN;
        gnss_data_fix.lastfix.updated_at = k_uptime_get_32();
        k_sem_reset(&warning_stop_sem);

        /* Update correction */
        process_correction(collar_mode, &gnss_data_fix.lastfix, fence_status, 
            current_zone, mean_dist, dist_change);

        /* Wait for correction ended event */
        zassert_equal(k_sem_take(&warning_stop_sem, K_SECONDS(5)), 0, "");

        /* Check that correction status is 0, e.g. correction not started */
        zassert_equal(get_correction_status(), 0, "");

        /* Wait for correction pause timeout */
        k_sleep(K_SECONDS(CORRECTION_PAUSE_MIN_TIME + 1)); 
    }
}

void test_correction_zone(void)
{
    /* 
     * Test that AMC correction is started/not started based on the correct 
     * collar modes accordingly. Correction shall only started when Collar Mode 
     * is Mode_Fence and Mode_Teach.
     */

	Mode collar_mode = Mode_Teach;
	FenceStatus fence_status = FenceStatus_FenceStatus_Normal;
	amc_zone_t current_zone = WARN_ZONE;
    uint16_t mean_dist = 0;
    uint16_t dist_change = 0;

    k_sleep(K_SECONDS(CORRECTION_PAUSE_MIN_TIME + 1));

    /* Enable and simulate a warning fix */
    amc_gnss_init();
    simulate_warn_fix();
    
    /*
     * Verify that all zones that should not start AMC correction does not.
     */
    for (int i = 0; i < WARN_ZONE; i++) {
        current_zone = i;
        
        gnss_data_fix.lastfix.updated_at = k_uptime_get_32();
        k_sem_reset(&warning_start_sem);

        /* Attempt to start correction */
        process_correction(collar_mode, &gnss_data_fix.lastfix, fence_status, 
            current_zone, mean_dist, dist_change);

        /* Wait for warning start event to timeout- no start event received */
        zassert_not_equal(k_sem_take(&warning_start_sem, K_SECONDS(5)), 0, "");

        /* Check that correction status is 0, e.g. correction not started */
        zassert_equal(get_correction_status(), 0, "");
    }

    current_zone = WARN_ZONE;
    gnss_data_fix.lastfix.updated_at = k_uptime_get_32();
    k_sem_reset(&warning_start_sem);

    /* Attempt to start correction */
    ztest_returns_value(get_active_delta, STATE_NORMAL);
    ztest_returns_value(stg_config_u32_write, 0);
    process_correction(collar_mode, &gnss_data_fix.lastfix, fence_status, 
        current_zone, mean_dist, dist_change);

    /* Wait for warning start event */
    zassert_equal(k_sem_take(&warning_start_sem, K_SECONDS(5)), 0, "");

    /* Check that correction status is 2, e.g. started and warning ON  */
    zassert_equal(get_correction_status(), 2, "");

    /* Stop correction by updating correction with Mode_Unknown */
    collar_mode = Mode_Mode_UNKNOWN;
    gnss_data_fix.lastfix.updated_at = k_uptime_get_32();
    k_sem_reset(&warning_stop_sem);

    /* Update correction */
    process_correction(collar_mode, &gnss_data_fix.lastfix, fence_status, 
        current_zone, mean_dist, dist_change);

    /* Wait for correction ended event */
    zassert_equal(k_sem_take(&warning_stop_sem, K_SECONDS(5)), 0, "");

    /* Check that correction status is 0, e.g. correction not started */
    zassert_equal(get_correction_status(), 0, "");
}

void test_correction_esacped(void)
{
    /* 
     * Test whether the AMC correction process is aborted when animal has 
     * escaped from the current pasture.
     */

	Mode collar_mode = Mode_Fence;
	FenceStatus fence_status = FenceStatus_FenceStatus_Normal;
	amc_zone_t current_zone = WARN_ZONE;
    uint16_t mean_dist = 0;
    uint16_t dist_change = 0;

    k_sleep(K_SECONDS(CORRECTION_PAUSE_MIN_TIME + 1));

    /* Enable GNSS and simulate a warning GNSS fix */
    amc_gnss_init();
    simulate_warn_fix();

    gnss_data_fix.lastfix.updated_at = k_uptime_get_32();
    k_sem_reset(&warning_start_sem);

    /* Start correction */
    ztest_returns_value(get_active_delta, STATE_NORMAL);
    ztest_returns_value(stg_config_u32_write, 0);
	process_correction(collar_mode, &gnss_data_fix.lastfix, fence_status, 
        current_zone, mean_dist, dist_change);

    /* Wait for warning start event */
	zassert_equal(k_sem_take(&warning_start_sem, K_SECONDS(5)), 0, "");

    /* Check that correction status is 2, e.g. started and warning ON  */
    zassert_equal(get_correction_status(), 2, "");

    /* Update correction with escaped fence status */
    gnss_data_fix.lastfix.updated_at = k_uptime_get_32();
    fence_status = FenceStatus_Escaped;

    k_sem_reset(&warning_stop_sem);

    /* Update correction */
	process_correction(collar_mode, &gnss_data_fix.lastfix, fence_status, 
        current_zone, mean_dist, dist_change);

    /* Wait for correction ended event (aborted due to Escpaed fence status) */
	zassert_equal(k_sem_take(&warning_stop_sem, K_SECONDS(5)), 0, "");

    /* Check that correction status is 0, e.g. correction not started */
    zassert_equal(get_correction_status(), 0, "");
}

void test_correction_active_delta(void)
{
    /* 
     * Test that active delta in sleep does not start correction 
     */

	Mode collar_mode = Mode_Fence;
	FenceStatus fence_status = FenceStatus_FenceStatus_Normal;
	amc_zone_t current_zone = WARN_ZONE;
    uint16_t mean_dist = 0;
    uint16_t dist_change = 0;

    /* Enable GNSS and simulate a warning GNSS fix */
    amc_gnss_init();
    simulate_warn_fix();

    gnss_data_fix.lastfix.updated_at = k_uptime_get_32();
    k_sem_reset(&warning_start_sem);

    /* Attempt to start correction */
    ztest_returns_value(get_active_delta, STATE_SLEEP);
	process_correction(collar_mode, &gnss_data_fix.lastfix, fence_status, 
        current_zone, mean_dist, dist_change);

    /* Wait for warning start event to timeout- correction should not start */
	zassert_not_equal(k_sem_take(&warning_start_sem, K_SECONDS(5)), 0, "");

    /* Check that correction status is 0, e.g. correction not started */
    zassert_equal(get_correction_status(), 0, "");
}

void test_correction_gnss(void)
{
    /*
     * Test that AMC correction is started, not started or paused based on the
     * state of GNSS data input.
     */

	Mode collar_mode = Mode_Fence;
	FenceStatus fence_status = FenceStatus_FenceStatus_Normal;
	amc_zone_t current_zone = WARN_ZONE;
    uint16_t mean_dist = 0;
    uint16_t dist_change = 0;

    amc_gnss_init();

    /*
     * Test 1
     * Test that an accepted GNSS fix does not start correction, only a warning
     * GNSS fix is accepted for correction to start.
     */

    /* Simulate an accepted GNSS fix */
    simulate_accepted_fix();
    gnss_data_fix.lastfix.updated_at = k_uptime_get_32();

    k_sem_reset(&warning_start_sem);

    /* Attempt to start correction */
    ztest_returns_value(get_active_delta, STATE_NORMAL);
	process_correction(collar_mode, &gnss_data_fix.lastfix, fence_status, 
        current_zone, mean_dist, dist_change);

    /* Wait for warning start event to timeout- no start event received */
	zassert_not_equal(k_sem_take(&warning_start_sem, K_SECONDS(5)), 0, "");

    /* Check that correction status is 0, e.g. correction not started */
    zassert_equal(get_correction_status(), 0, "");
    
    /* 
     * Test 2
     * Test that only a warning GNSS fix the GNSS mode "MAX" can start AMC 
     * correction. 
     */

    /* Simulate a warning GNSS fix */
    simulate_warn_fix();

    /* Verify that GNSS modes other than GNSSMODE_MAX cannot start correction */
    for (int i = 0; i < (GNSSMODE_SIZE - 1); i++) {
        gnss_data_fix.lastfix.mode = i;

        gnss_data_fix.lastfix.updated_at = k_uptime_get_32();
        k_sem_reset(&warning_start_sem);

        process_correction(collar_mode, &gnss_data_fix.lastfix, fence_status, 
            current_zone, mean_dist, dist_change);

        /* Wait for warning start event to timeout- no start event received */
        zassert_not_equal(k_sem_take(&warning_start_sem, K_SECONDS(5)), 0, "");

        /* Check that correction status is 0, e.g. correction not started */
        zassert_equal(get_correction_status(), 0, "");
    }

    gnss_data_fix.lastfix.mode = GNSSMODE_MAX;
    gnss_data_fix.lastfix.updated_at = k_uptime_get_32();

    k_sem_reset(&warning_start_sem);
    k_sem_reset(&sound_event_sem);
    k_sem_reset(&animal_warning_sem);
    k_sem_reset(&warning_freq_sem);

    /* Start correction */
    ztest_returns_value(get_active_delta, STATE_NORMAL);
    ztest_returns_value(stg_config_u32_write, 0);
	process_correction(collar_mode, &gnss_data_fix.lastfix, fence_status, 
        current_zone, mean_dist, dist_change);

    /* Wait for warning start event */
	zassert_equal(k_sem_take(&warning_start_sem, K_SECONDS(5)), 0, "");

    /* Wait for sound event and check that warning sound is turned ON */
	zassert_equal(k_sem_take(&sound_event_sem, K_SECONDS(5)), 0, "");
    zassert_equal(m_sound_type, SND_WARN, "");

    /* Wait for animal warning event */
	zassert_equal(k_sem_take(&animal_warning_sem, K_SECONDS(5)), 0, "");

    /* Wait for sound frequency event and check that frequency is INIT */
	zassert_equal(k_sem_take(&warning_freq_sem, K_SECONDS(5)), 0, "");
    zassert_equal(m_warning_freq, convert_to_legacy_freq(WARN_FREQ_INIT), "");

    /* Check that correction status is 2, e.g. started and warning ON  */
    zassert_equal(get_correction_status(), 2, "");

    /*
     * Test 3
     * Update correction with old GNSS data. This should pause AMC correction.
     */

    /* Update GNSS data with old data */
    gnss_data_fix.lastfix.updated_at = k_uptime_get_32() - 1500;

    k_sem_reset(&sound_event_sem);
    k_sem_reset(&warning_pause_sem);

    /* Update correction */
    ztest_returns_value(get_active_delta, STATE_NORMAL);
	process_correction(collar_mode, &gnss_data_fix.lastfix, fence_status, 
        current_zone, mean_dist, dist_change);

    /* Wait for sound event and check that warning sound is turned OFF */
    zassert_equal(k_sem_take(&sound_event_sem, K_SECONDS(5)), 0, "");
    zassert_equal(m_sound_type, SND_OFF, "");

    /* Wait for correction paused event (paused due to old GNSS data) */
	zassert_equal(k_sem_take(&warning_pause_sem, K_SECONDS(5)), 0, "");

    /* Check that correction status is 1, e.g. started, but warning OFF */
    zassert_equal(get_correction_status(), 1, "");

    /*
     * Test 4
     * Update correction with new GNSS data immediately. This should not resume
     * correction as the correction must be paused for at least 3 seconds
     * before it can be resumed (see CORRECTION_PAUSE_MIN_TIME)
     */

    /* Update GNSS data with valid timestamp */
    gnss_data_fix.lastfix.updated_at = k_uptime_get_32();

    k_sem_reset(&sound_event_sem);
    k_sem_reset(&animal_warning_sem);
    k_sem_reset(&warning_freq_sem);

    /* Update correction */
    ztest_returns_value(get_active_delta, STATE_NORMAL);
	process_correction(collar_mode, &gnss_data_fix.lastfix, fence_status, 
        current_zone, mean_dist, dist_change);

    /* Wait for event to timeout indicating that correct was NOT resumed */
    zassert_not_equal(k_sem_take(&sound_event_sem, K_SECONDS(5)), 0, "");

    /* Check that correction status is 1, e.g. started, but warning OFF */
    zassert_equal(get_correction_status(), 1, "");

    /* Wait for correction pause timeout */
    k_sleep(K_SECONDS(CORRECTION_PAUSE_MIN_TIME));

    gnss_data_fix.lastfix.updated_at = k_uptime_get_32();
    k_sem_reset(&sound_event_sem);

    /* Update correction */
    ztest_returns_value(get_active_delta, STATE_NORMAL);
    ztest_returns_value(stg_config_u32_write, 0);
	process_correction(collar_mode, &gnss_data_fix.lastfix, fence_status, 
        current_zone, mean_dist, dist_change);

    /* Wait for sound event and check that warning sound is turned ON */
    zassert_equal(k_sem_take(&sound_event_sem, K_SECONDS(5)), 0, "");
    zassert_equal(m_sound_type, SND_WARN, "");

    /* Wait for animal warning event */
	zassert_equal(k_sem_take(&animal_warning_sem, K_SECONDS(5)), 0, "");

    /* Wait for sound frequency event and check that frequency is INIT value */
    zassert_equal(k_sem_take(&warning_freq_sem, K_SECONDS(5)), 0, "");
    zassert_equal(m_warning_freq, convert_to_legacy_freq(WARN_FREQ_INIT), "");

    /* Check that correction status is 2, e.g. started and warning ON */
    zassert_equal(get_correction_status(), 2, "");

    /* Simulate no GNSS fix- this should pause AMC correction */
    simulate_no_fix();
    gnss_data_fix.lastfix.updated_at = k_uptime_get_32();

    k_sem_reset(&warning_pause_sem);

    /* Update correction */
    ztest_returns_value(get_active_delta, STATE_NORMAL);
	process_correction(collar_mode, &gnss_data_fix.lastfix, fence_status, 
        current_zone, mean_dist, dist_change);

    /* Wait for correction paused event (paused due to no GNSS fix) */
	zassert_equal(k_sem_take(&warning_pause_sem, K_SECONDS(5)), 0, "");

    /* Check that correction status is 1, e.g. started but warning OFF */
    zassert_equal(get_correction_status(), 1, "");

    /* Stop correction by setting collar mode to Mode_Unknown*/
    collar_mode = Mode_Mode_UNKNOWN;
    gnss_data_fix.lastfix.updated_at = k_uptime_get_32();

    k_sem_reset(&warning_stop_sem);

    /* Update correction */
	process_correction(collar_mode, &gnss_data_fix.lastfix, fence_status, 
        current_zone, mean_dist, dist_change);

    /* Wait for correction ended event */
    zassert_equal(k_sem_take(&warning_stop_sem, K_SECONDS(5)), 0, "");

    /* Check that correction status is 0, e.g. correction not started */
    zassert_equal(get_correction_status(), 0, "");
}

void test_correction_dist_pause(void)
{
    /* 
     * Test that AMC correction fence distance input (mean distance and distance 
     * change) pause/resume AMC correction based on thresholds.
     */

	Mode collar_mode = Mode_Fence;
	FenceStatus fence_status = FenceStatus_FenceStatus_Normal;
	amc_zone_t current_zone = WARN_ZONE;
    int16_t mean_dist = 0;
    int16_t dist_change = 0;

    /* Enable GNSS and simulate a warning GNSS fix */
    amc_gnss_init();
    simulate_warn_fix();

    gnss_data_fix.lastfix.updated_at = k_uptime_get_32();
    k_sem_reset(&warning_start_sem);

    /* Attempt to start correction */
    ztest_returns_value(get_active_delta, STATE_NORMAL);
    ztest_returns_value(stg_config_u32_write, 0);
	process_correction(collar_mode, &gnss_data_fix.lastfix, fence_status, 
        current_zone, mean_dist, dist_change);

    /* Wait for warning start event */
	zassert_equal(k_sem_take(&warning_start_sem, K_SECONDS(5)), 0, "");

    /* Check that correction status is 2, e.g. started and warning ON */
    zassert_equal(get_correction_status(), 2, "");

    /* Check that correction is paused when mean_dist - last_warn_dist <= 
     * CORRECTION_PAUSE_DIST, but not otherwise, for Mode_Fence  */
    gnss_data_fix.lastfix.updated_at = k_uptime_get_32();

    /* Update correction with mean distance -15 (last dist = 0). This should
     * result in no change */
    ztest_returns_value(get_active_delta, STATE_NORMAL);
	process_correction(collar_mode, &gnss_data_fix.lastfix, fence_status, 
        current_zone, -15, dist_change);

    /* Check that correction status is 2, e.g. started and warning ON */
    zassert_equal(get_correction_status(), 2, "");

    gnss_data_fix.lastfix.updated_at = k_uptime_get_32();
    k_sem_reset(&warning_pause_sem);

    /* Update correction with mean distance -25. This should result in a 
     * correction pause */
    ztest_returns_value(get_active_delta, STATE_NORMAL);
	process_correction(collar_mode, &gnss_data_fix.lastfix, fence_status, 
        current_zone, -25, dist_change);

    /* Wait for correction paused event */
	zassert_equal(k_sem_take(&warning_pause_sem, K_SECONDS(5)), 0, "");

    /* Check that correction status is 1, e.g. started but warning OFF */
    zassert_equal(get_correction_status(), 1, "");

    /* Wait for correction pause timeout */
    k_sleep(K_SECONDS(CORRECTION_PAUSE_MIN_TIME + 1));

    gnss_data_fix.lastfix.updated_at = k_uptime_get_32();
    k_sem_reset(&sound_event_sem);
    k_sem_reset(&animal_warning_sem);
    k_sem_reset(&warning_freq_sem);

    /* Resume correction */
    ztest_returns_value(get_active_delta, STATE_NORMAL);
    ztest_returns_value(stg_config_u32_write, 0);
	process_correction(collar_mode, &gnss_data_fix.lastfix, fence_status, 
        current_zone, 0, dist_change);

    /* Wait for sound event and check that warning sound is turned ON */
    zassert_equal(k_sem_take(&sound_event_sem, K_SECONDS(5)), 0, "");
    zassert_equal(m_sound_type, SND_WARN, "");

    /* Wait for animal warning event */
	zassert_equal(k_sem_take(&animal_warning_sem, K_SECONDS(5)), 0, "");

    /* Wait for sound frequency event and check that frequency is ... */
    zassert_equal(k_sem_take(&warning_freq_sem, K_SECONDS(5)), 0, "");
    zassert_equal(m_warning_freq, convert_to_legacy_freq(WARN_FREQ_INIT), "");

    /* Check that correction status is 2, e.g. started and warning ON */
    zassert_equal(get_correction_status(), 2, ""); 

    /*
     * Check that correction is pasued for mean_dist - last_warn_dist <=
	 * TEACHMODE_CORRECTION_PAUSE_DIST, not not otherwise, for Mode_Teach.
     */
    collar_mode = Mode_Teach;
    gnss_data_fix.lastfix.updated_at = k_uptime_get_32();
    
    /* Update correction with mean dist -5. This should not pause correction */
    ztest_returns_value(get_active_delta, STATE_NORMAL);
	process_correction(collar_mode, &gnss_data_fix.lastfix, fence_status, 
        current_zone, -5, dist_change);

    /* Check that correction status is 2, e.g. started and warning ON */
    zassert_equal(get_correction_status(), 2, "");

    gnss_data_fix.lastfix.updated_at = k_uptime_get_32();
    k_sem_reset(&warning_pause_sem);

    /* Update correction with mean dist -15. This should pause correction */
    ztest_returns_value(get_active_delta, STATE_NORMAL);
	process_correction(collar_mode, &gnss_data_fix.lastfix, fence_status, 
        current_zone, -15, dist_change);

    /* Wait for correction paused event */
	zassert_equal(k_sem_take(&warning_pause_sem, K_SECONDS(5)), 0, "");

    /* Check that correction status is 1, e.g. started but warning OFF */
    zassert_equal(get_correction_status(), 1, "");

    /* Wait for correction pause timeout */
    k_sleep(K_SECONDS(CORRECTION_PAUSE_MIN_TIME + 1));

    gnss_data_fix.lastfix.updated_at = k_uptime_get_32();

    /* Resume correction */
    ztest_returns_value(get_active_delta, STATE_NORMAL);
    ztest_returns_value(stg_config_u32_write, 0);
	process_correction(collar_mode, &gnss_data_fix.lastfix, fence_status, 
        current_zone, 0, dist_change);

    /* Check that correction status is 2, e.g. started and warning ON */
    zassert_equal(get_correction_status(), 2, ""); 

    /*
     * Check that dist_change <= TEACHMODE_DIST_DECR_SLOPE_OFF_LIM will pause
     * the correction.
     */

    gnss_data_fix.lastfix.updated_at = k_uptime_get_32();
    k_sem_reset(&warning_pause_sem);

    /* Update correction with dist_change -7- this should pause correction */
    ztest_returns_value(get_active_delta, STATE_NORMAL);
	process_correction(collar_mode, &gnss_data_fix.lastfix, fence_status, 
        current_zone, 0, -7);

    /* Wait for correction paused event */
	zassert_equal(k_sem_take(&warning_pause_sem, K_SECONDS(5)), 0, "");

    /* Check that correction status is 1, e.g. started but warning OFF */
    zassert_equal(get_correction_status(), 1, "");
}

static bool event_handler(const struct event_header *evt)
{
	if (is_warn_correction_start_event(evt)) 
    {
        k_sem_give(&warning_start_sem);
		return false;
	}
	if (is_warn_correction_pause_event(evt))
    {
        k_sem_give(&warning_pause_sem);
		return false;
	}
	if (is_warn_correction_end_event(evt))
    {
        k_sem_give(&warning_stop_sem);
		return false;
	}
	if (is_animal_warning_event(evt)) 
    {
        k_sem_give(&animal_warning_sem);
		return false;
	}
	if (is_sound_event(evt)) 
    {
		struct sound_event *sound_evt = cast_sound_event(evt);
		m_sound_type = sound_evt->type; 
        k_sem_give(&sound_event_sem);
		return false;
	}
	if (is_sound_set_warn_freq_event(evt))
    {
        struct sound_set_warn_freq_event *warn_freq_evt = 
            cast_sound_set_warn_freq_event(evt);
        m_warning_freq = warn_freq_evt->freq;
        k_sem_give(&warning_freq_sem);
		return false;
	}
	if (is_ep_status_event(evt)) 
    {
		return false;
	}
	if (is_amc_zapped_now_event(evt)) 
    {
		return false;
	}
	return false;
}

EVENT_LISTENER(test_correction_handler, event_handler);
EVENT_SUBSCRIBE(test_correction_handler, warn_correction_start_event);
EVENT_SUBSCRIBE(test_correction_handler, warn_correction_pause_event);
EVENT_SUBSCRIBE(test_correction_handler, warn_correction_end_event);
EVENT_SUBSCRIBE(test_correction_handler, animal_warning_event);
EVENT_SUBSCRIBE(test_correction_handler, sound_event);
EVENT_SUBSCRIBE(test_correction_handler, sound_set_warn_freq_event);
EVENT_SUBSCRIBE(test_correction_handler, ep_status_event);
EVENT_SUBSCRIBE(test_correction_handler, amc_zapped_now_event);

static uint32_t convert_to_legacy_freq(uint32_t freq)
{
	uint8_t ocr_value = (4000000 / (2 * 32 * freq)) - 1;
	freq = 4000000 / ((ocr_value + 1) * 2 * 32);

	if (freq > WARN_FREQ_MAX) 
    {
		freq = WARN_FREQ_MAX;
	} 
    else if (freq < WARN_FREQ_INIT) 
    {
		freq = WARN_FREQ_INIT;
	}
	return freq;
}
