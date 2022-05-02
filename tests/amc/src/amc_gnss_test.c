#include "amc_test_common.h"
#include "amc_gnss.h"
#include <ztest.h>
#include "amc_states_cache.h"
#include "embedded.pb.h"
#include "gnss_controller_events.h"

/* Signals GNSS fix timeout */
static struct k_sem gnss_fix_timeout;

static int gnss_timeout(void)
{
	k_sem_give(&gnss_fix_timeout);

	return 0;
}

void test_gnss_fix(void)
{
	k_sem_init(&gnss_fix_timeout, 0, 1);

	zassert_equal(gnss_init(gnss_timeout), 0,
		      "Failed initializing AMC GNSS");

	gnss_t gnss_data = { .latest = { .pvt_flags = 1 },
			     .fix_ok = true,
			     .lastfix = { .pvt_flags = 1 },
			     .has_lastfix = true };

	/* Send GNSS data, has fix, but not much more */
	zassert_equal(gnss_update(&gnss_data), 0, "Failed updating GNSS fix");

	/* Verify state of fix */
	zassert_true(gnss_has_fix(), "Did not have fix");
	zassert_false(gnss_has_accepted_fix(), "Did have accepted fix");
	zassert_false(gnss_has_easy_fix(), "Did have easy fix");

	/* Wait for timeout */
	zassert_equal(k_sem_take(&gnss_fix_timeout, K_MSEC(2000)), 0,
		      "GNSS fix did not timeout as expected");

	/* But no more timeouts for now */
	zassert_equal(k_sem_take(&gnss_fix_timeout, K_MSEC(30000)), -EAGAIN,
		      "GNSS fix timed out more than once");

	/* Lost fix */
	gnss_data.latest.pvt_flags = 0;
	zassert_equal(gnss_update(&gnss_data), 0, "Failed updating GNSS fix");

	/* Verify state of no fix */
	zassert_false(gnss_has_fix(), "Did have fix");
	zassert_false(gnss_has_accepted_fix(), "Did have accepted fix");
	zassert_false(gnss_has_easy_fix(), "Did have easy fix");

	/* Easy fix */
	gnss_data.latest.pvt_flags = 1;
	gnss_data.latest.num_sv = 7;
	gnss_data.latest.h_dop = 80;
	gnss_data.latest.h_acc_dm = 65;
	gnss_data.latest.height = 100;
	zassert_equal(gnss_update(&gnss_data), 0, "Failed updating GNSS fix");

	/* Verify state of easy fix */
	zassert_true(gnss_has_fix(), "Did not have fix");
	zassert_false(gnss_has_accepted_fix(), "Did have accepted fix");
	zassert_true(gnss_has_easy_fix(), "Did not have easy fix");

	/* Accepted fix */
	gnss_data.latest.pvt_flags = 1;
	gnss_data.latest.num_sv = 7;
	gnss_data.latest.h_dop = 80;
	gnss_data.latest.h_acc_dm = 25;
	gnss_data.latest.height = 100;
	zassert_equal(gnss_update(&gnss_data), 0, "Failed updating GNSS fix");

	/* Verify state of accepted fix */
	zassert_true(gnss_has_fix(), "Did not have fix");
	zassert_true(gnss_has_accepted_fix(), "Did not have accepted fix");
	zassert_true(gnss_has_easy_fix(), "Did not have easy fix");

	/* Verify reset of easy fix on timeout */
	/* Wait for timeout */
	zassert_equal(k_sem_take(&gnss_fix_timeout, K_MSEC(2000)), 0,
		      "GNSS fix did not timeout as expected");

	/* Verify state of easy fix */
	zassert_true(gnss_has_fix(), "Did not have fix");
	zassert_true(gnss_has_accepted_fix(), "Did not have accepted fix");
	zassert_false(gnss_has_easy_fix(), "Did not have easy fix");
}

K_SEM_DEFINE(gnss_mode_sem, 0, 1);
static gnss_mode_t current_gnss_mode = GNSSMODE_INACTIVE;

void test_gnss_mode(void)
{
	/* Caution state with unknown. */
	FenceStatus fs = FenceStatus_FenceStatus_UNKNOWN;
	CollarStatus cs = CollarStatus_CollarStatus_UNKNOWN;
	Mode mode = Mode_Mode_UNKNOWN;
	amc_zone_t zone = NO_ZONE;

	set_sensor_modes(mode, fs, cs, zone);
	zassert_false(k_sem_take(&gnss_mode_sem, K_SECONDS(30)), "");
	zassert_equal(current_gnss_mode, GNSSMODE_CAUTION, "");

	/* MAX mode. */
	//k_sem_give(&gnss_mode_sem);
	//fs = FenceStatus_FenceStatus_Normal;
	//mode = Mode_Teach;
	//zone = WARN_ZONE;
	//
	//set_sensor_modes(mode, fs, cs, zone);
	//zassert_false(k_sem_take(&gnss_mode_sem, K_SECONDS(30)), "");
	//zassert_equal(current_gnss_mode, GNSSMODE_MAX, "");
}

static bool event_handler(const struct event_header *eh)
{
	if (is_gnss_set_mode_event(eh)) {
		struct gnss_set_mode_event *ev = cast_gnss_set_mode_event(eh);
		current_gnss_mode = ev->mode;
		k_sem_give(&gnss_mode_sem);
	}
	return false;
}

EVENT_LISTENER(test_gnss_handler, event_handler);
EVENT_SUBSCRIBE(test_gnss_handler, gnss_set_mode_event);