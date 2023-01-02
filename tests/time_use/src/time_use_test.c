/*
* Copyright (c) 2022 Nofence AS
*/

#include <ztest.h>
#include <time_use.h>
#include <amc_gnss.h>
#include <event_manager.h>
#include <amc_events.h>
#include <histogram_events.h>
#include <messaging_module_events.h>
#include <cellular_controller_events.h>
#include <movement_events.h>
#include <gnss_controller_events.h>
#include <pwr_event.h>

#define SLACK CONFIG_TIME_USE_RESOLUTION_MS

#ifdef TEST_VERBOSE
void event_manager_trace_event_execution(const struct event_header *eh,
						bool is_start)
{
	printk("Executing %s\n",eh->type_id->name);
}

void event_manager_trace_event_submission(const struct event_header *eh,
						 const void *trace_info)
{
	printk("Submitting %s\n",eh->type_id->name);
}
#endif


/* mocks */
bool gnss_has_accepted_fix(void)
{
	return true;
}

bool gnss_has_easy_fix(void)
{
	return true;
}

static void reset_stats_bufs_now(collar_histogram * histogram) {
	memset(histogram,0,sizeof(collar_histogram));
	k_msgq_purge(&histogram_msgq);
	struct save_histogram * save_ev = new_save_histogram();
	EVENT_SUBMIT(save_ev);
	int err = k_msgq_get(&histogram_msgq, histogram, K_SECONDS(10));
	zassert_equal(err,0,"");
}

static void test_zones()
{

	collar_histogram histogram;
	reset_stats_bufs_now(&histogram);

	struct zone_change *ev_zone_change = new_zone_change();
	ev_zone_change->zone = NO_ZONE;
	EVENT_SUBMIT(ev_zone_change);
	k_sleep(K_SECONDS(10));

	ev_zone_change = new_zone_change();
	ev_zone_change->zone = PSM_ZONE;
	EVENT_SUBMIT(ev_zone_change);
	k_sleep(K_SECONDS(100));

	ev_zone_change = new_zone_change();
	ev_zone_change->zone = CAUTION_ZONE;
	EVENT_SUBMIT(ev_zone_change);
	k_sleep(K_SECONDS(200));

	ev_zone_change = new_zone_change();
	ev_zone_change->zone = PREWARN_ZONE;
	EVENT_SUBMIT(ev_zone_change);
	k_sleep(K_SECONDS(300));

	ev_zone_change = new_zone_change();
	ev_zone_change->zone = WARN_ZONE;
	EVENT_SUBMIT(ev_zone_change);
	k_sleep(K_SECONDS(400));

	ev_zone_change = new_zone_change();
	ev_zone_change->zone = NO_ZONE;
	EVENT_SUBMIT(ev_zone_change);

	reset_stats_bufs_now(&histogram);

	zassert_within(histogram.in_zone.usPSMZoneTime,100,2,"");
	zassert_within(histogram.in_zone.usCAUTIONZoneTime,200,2,"");
	zassert_within(histogram.in_zone.usMAXZoneTime,300+400,2,"");
}

static void test_historgram_current_profile() {

	collar_histogram histogram;
	reset_stats_bufs_now(&histogram);

	/* Test that if the animal is sleeping, we get the expected statistics */
	struct update_collar_status *ev_collar_status = new_update_collar_status();
	struct update_fence_status *ev_fence_status = new_update_fence_status();
	ev_collar_status->collar_status = CollarStatus_Sleep;
	EVENT_SUBMIT(ev_collar_status);
	ev_fence_status->fence_status = FenceStatus_BeaconContactNormal;
	EVENT_SUBMIT(ev_fence_status);
	reset_stats_bufs_now(&histogram);
	k_sleep(K_SECONDS(100));
	reset_stats_bufs_now(&histogram);
	zassert_within(histogram.current_profile.usCC_Sleep,100,SLACK,"");
	zassert_within(histogram.current_profile.usCC_BeaconZone,0,1,"");
	zassert_within(histogram.current_profile.usCC_GNSS_SuperE_POT,0,1,"");
	zassert_within(histogram.current_profile.usCC_GNSS_SuperE_Acquition,0,1,"");
	zassert_within(histogram.current_profile.usCC_GNSS_SuperE_Tracking,0,1,"");
	zassert_within(histogram.current_profile.usCC_GNSS_MAX,0,1,"");
	zassert_within(histogram.current_profile.usCC_Modem_Active,100,SLACK,"");

	/* Test that if animal is in beacon zone, we get the correct statistics */
	ev_collar_status = new_update_collar_status();
	ev_collar_status->collar_status = CollarStatus_CollarStatus_Normal;
	EVENT_SUBMIT(ev_collar_status);
	reset_stats_bufs_now(&histogram);
	k_sleep(K_SECONDS(100));
	reset_stats_bufs_now(&histogram);
	zassert_within(histogram.current_profile.usCC_Sleep,0,1,"");
	zassert_within(histogram.current_profile.usCC_BeaconZone,100,SLACK,"");
	zassert_within(histogram.current_profile.usCC_GNSS_SuperE_POT,0,1,"");
	zassert_within(histogram.current_profile.usCC_GNSS_SuperE_Acquition,0,1,"");
	zassert_within(histogram.current_profile.usCC_GNSS_SuperE_Tracking,0,1,"");
	zassert_within(histogram.current_profile.usCC_GNSS_MAX,0,1,"");
	zassert_within(histogram.current_profile.usCC_Modem_Active,100,SLACK,"");

	/* Test that the modem time is reported consistently */
	struct modem_state * modem_state_event = new_modem_state();
	modem_state_event->mode = POWER_OFF;
	EVENT_SUBMIT(modem_state_event);
	reset_stats_bufs_now(&histogram);
	k_sleep(K_SECONDS(100));
	reset_stats_bufs_now(&histogram);
	zassert_within(histogram.current_profile.usCC_Modem_Active,0,1,"");

}

static void test_animal_behave()
{
	collar_histogram histogram;
	struct activity_level * ev = new_activity_level();
	ev->level = ACTIVITY_NO;
	EVENT_SUBMIT(ev);
	reset_stats_bufs_now(&histogram);
	k_sleep(K_SECONDS(100));
	reset_stats_bufs_now(&histogram);

	zassert_within(histogram.animal_behave.usRestingTime,100,SLACK,"");
	zassert_false(histogram.animal_behave.has_usWalkingTime,"");
	zassert_false(histogram.animal_behave.has_usRunningTime,"");
	zassert_false(histogram.animal_behave.has_usWalkingDist,"");
	zassert_false(histogram.animal_behave.has_usRunningDist,"");

	ev = new_activity_level();
	ev->level = ACTIVITY_MED;
	EVENT_SUBMIT(ev);
	reset_stats_bufs_now(&histogram);
	k_sleep(K_SECONDS(100));
	reset_stats_bufs_now(&histogram);
	zassert_false(histogram.animal_behave.has_usRestingTime,"");
	zassert_within(histogram.animal_behave.usWalkingTime,100,SLACK,"");
	zassert_false(histogram.animal_behave.has_usRunningTime,"");
	zassert_false(histogram.animal_behave.has_usWalkingDist,"");
	zassert_false(histogram.animal_behave.has_usRunningDist,"");


	ev = new_activity_level();
	ev->level = ACTIVITY_HIGH;
	EVENT_SUBMIT(ev);
	reset_stats_bufs_now(&histogram);
	k_sleep(K_SECONDS(100));
	reset_stats_bufs_now(&histogram);
	zassert_false(histogram.animal_behave.has_usRestingTime,"");
	zassert_false(histogram.animal_behave.has_usWalkingTime,"");
	zassert_within(histogram.animal_behave.usRunningTime,100,SLACK,"");
	zassert_false(histogram.animal_behave.has_usWalkingDist,"");
	zassert_false(histogram.animal_behave.has_usRunningDist,"");


	/* Step counter, check thread critical sections */
	reset_stats_bufs_now(&histogram);
	struct step_counter_event * sc_e = new_step_counter_event();
	sc_e->steps = 100;
	EVENT_SUBMIT(ev);
	k_yield();
	sc_e = new_step_counter_event();
	sc_e->steps = 200;
	EVENT_SUBMIT(ev);
	k_sleep(K_SECONDS(10));
	reset_stats_bufs_now(&histogram);
	zassert_equal(histogram.animal_behave.usStepCounter,300,"");

	/* Walking/running distances*/
	ev = new_activity_level();
	ev->level = ACTIVITY_HIGH;
	EVENT_SUBMIT(ev);
	reset_stats_bufs_now(&histogram);
	struct xy_location * xy;
	xy = new_xy_location();
	xy->x = 0;
	xy->y = 0;
	EVENT_SUBMIT(xy);
	k_sleep(K_SECONDS(100));
	reset_stats_bufs_now(&histogram);
	zassert_false(histogram.animal_behave.has_usWalkingDist,"");
	zassert_false(histogram.animal_behave.has_usRunningDist,"");
	xy = new_xy_location();
	xy->x = 0;
	xy->y = 0;
	EVENT_SUBMIT(xy);
	/* must let the collector thread run for one iteration to register the first waypoint */
	k_sleep(K_SECONDS(10));
	xy = new_xy_location();
	xy->x = 100;
	xy->y = 0;
	EVENT_SUBMIT(xy);
	k_sleep(K_SECONDS(100));
	reset_stats_bufs_now(&histogram);
	zassert_false(histogram.animal_behave.has_usWalkingDist,"");
	zassert_true(histogram.animal_behave.has_usRunningDist,"");
	zassert_equal(histogram.animal_behave.usRunningDist,100,"");

}

static void test_gnss_baro()
{
	collar_histogram histogram;
	/* clear  */
	reset_stats_bufs_now(&histogram);
	/* test default values */
	k_sleep(K_SECONDS(10));
	reset_stats_bufs_now(&histogram);
	zassert_equal(histogram.qc_baro_gps_max_mean_min.usGpsHeightMin,UINT16_MAX,"");
	zassert_equal(histogram.qc_baro_gps_max_mean_min.usGpsHeightMax,0,"");
	zassert_equal(histogram.qc_baro_gps_max_mean_min.usGpsHeightMean,0,"");
	zassert_equal(histogram.qc_baro_gps_max_mean_min.usGpsSpeedMin,UINT16_MAX,"");
	zassert_equal(histogram.qc_baro_gps_max_mean_min.usGpsSpeedMax,0,"");
	zassert_equal(histogram.qc_baro_gps_max_mean_min.usGpsSpeedMean,0,"");

	struct gnss_data * ev = new_gnss_data();

	ev->gnss_data.fix_ok = true;
	ev->gnss_data.latest.mode = GNSSMODE_CAUTION;
	ev->gnss_data.latest.pvt_flags = 0;
	ev->gnss_data.latest.height = 100;
	ev->gnss_data.latest.speed = 200;
	EVENT_SUBMIT(ev);
	k_yield();
	ev = new_gnss_data();
	ev->gnss_data.fix_ok = true;
	ev->gnss_data.latest.mode = GNSSMODE_CAUTION;
	ev->gnss_data.latest.pvt_flags = 0;
	ev->gnss_data.latest.height = 200;
	ev->gnss_data.latest.speed = 400;
	EVENT_SUBMIT(ev);
	k_sleep(K_SECONDS(10));
	reset_stats_bufs_now(&histogram);
	zassert_equal(histogram.qc_baro_gps_max_mean_min.usGpsHeightMin,100,"");
	zassert_equal(histogram.qc_baro_gps_max_mean_min.usGpsHeightMax,200,"");
	zassert_equal(histogram.qc_baro_gps_max_mean_min.usGpsHeightMean,150,"");
	zassert_equal(histogram.qc_baro_gps_max_mean_min.usGpsSpeedMin,200,"");
	zassert_equal(histogram.qc_baro_gps_max_mean_min.usGpsSpeedMax,400,"");
	zassert_equal(histogram.qc_baro_gps_max_mean_min.usGpsSpeedMean,300,"");

	/* test that a new collection does not use old variables */
	ev = new_gnss_data();
	ev->gnss_data.fix_ok = true;
	ev->gnss_data.latest.mode = GNSSMODE_CAUTION;
	ev->gnss_data.latest.pvt_flags = 0;
	ev->gnss_data.latest.height = 120;
	ev->gnss_data.latest.speed = 220;
	EVENT_SUBMIT(ev);
	k_yield();
	ev = new_gnss_data();
	ev->gnss_data.fix_ok = true;
	ev->gnss_data.latest.mode = GNSSMODE_CAUTION;
	ev->gnss_data.latest.pvt_flags = 0;
	ev->gnss_data.latest.height = 190;
	ev->gnss_data.latest.speed = 390;
	EVENT_SUBMIT(ev);
	k_sleep(K_SECONDS(10));
	reset_stats_bufs_now(&histogram);
	zassert_equal(histogram.qc_baro_gps_max_mean_min.usGpsHeightMin,120,"");
	zassert_equal(histogram.qc_baro_gps_max_mean_min.usGpsHeightMax,190,"");
	zassert_equal(histogram.qc_baro_gps_max_mean_min.usGpsHeightMean,155,"");
	zassert_equal(histogram.qc_baro_gps_max_mean_min.usGpsSpeedMin,220,"");
	zassert_equal(histogram.qc_baro_gps_max_mean_min.usGpsSpeedMax,390,"");
	zassert_equal(histogram.qc_baro_gps_max_mean_min.usGpsSpeedMean,305,"");

}

static void test_gnss_mode_switches()
{
	collar_histogram histogram;
	reset_stats_bufs_now(&histogram);
	struct gnss_set_mode_event *ev = new_gnss_set_mode_event();
	ev->mode = GNSSMODE_INACTIVE;
	EVENT_SUBMIT(ev);
	k_yield();
	ev = new_gnss_set_mode_event();
	ev->mode = GNSSMODE_PSM;
	EVENT_SUBMIT(ev);
	k_yield();
	ev = new_gnss_set_mode_event();
	ev->mode = GNSSMODE_CAUTION;
	EVENT_SUBMIT(ev);
	k_yield();
	ev = new_gnss_set_mode_event();
	ev->mode = GNSSMODE_MAX;
	EVENT_SUBMIT(ev);
	k_yield();
	k_sleep(K_SECONDS(10));
	reset_stats_bufs_now(&histogram);
	zassert_equal(histogram.gnss_modes.usUnknownCount, 0, "");
	zassert_equal(histogram.gnss_modes.usOffCount, 1, "");
	zassert_equal(histogram.gnss_modes.usPSMCount, 1, "");
	zassert_equal(histogram.gnss_modes.usCautionCount, 1, "");
	zassert_equal(histogram.gnss_modes.usMaxCount, 1, "");
	/* Ensure that variables are reset */
	k_sleep(K_SECONDS(10));
	reset_stats_bufs_now(&histogram);
	zassert_equal(histogram.gnss_modes.usUnknownCount, 0, "");
	zassert_equal(histogram.gnss_modes.usOffCount, 0, "");
	zassert_equal(histogram.gnss_modes.usPSMCount, 0, "");
	zassert_equal(histogram.gnss_modes.usCautionCount, 0, "");
	zassert_equal(histogram.gnss_modes.usMaxCount, 0, "");
}

static void test_battery_qc()
{
	collar_histogram histogram;
	reset_stats_bufs_now(&histogram);
	struct pwr_status_event *ev;
	ev = new_pwr_status_event();
	ev->battery_mv = 4000;
	ev->battery_mv_min = 3000;
	ev->battery_mv_max = 5000;
	EVENT_SUBMIT(ev);
	k_yield();
	ev = new_pwr_status_event();
	ev->battery_mv = 3500;
	ev->battery_mv_min = 3490;
	ev->battery_mv_max = 3510;
	EVENT_SUBMIT(ev);
	k_yield();

	k_sleep(K_SECONDS(10));
	reset_stats_bufs_now(&histogram);
	zassert_equal(histogram.qc_battery.usVbattMin, 3000/10, "");
	zassert_equal(histogram.qc_battery.usVbattMax, 5000/10, "");
}

void test_main(void)
{
	time_use_module_init();
	ztest_test_suite(timeuse_tests,
			 ztest_unit_test(test_zones),
			 ztest_unit_test(test_historgram_current_profile),
			 ztest_unit_test(test_animal_behave),
			 ztest_unit_test(test_gnss_baro),
			 ztest_unit_test(test_gnss_mode_switches),
			 ztest_unit_test(test_battery_qc)

	);

	ztest_run_test_suite(timeuse_tests);
}