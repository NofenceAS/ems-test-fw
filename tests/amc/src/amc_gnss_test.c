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

static gnss_t gnss_data = { .latest = { .pvt_flags = 1 },
			    .fix_ok = true,
			    .lastfix = { .pvt_flags = 1 },
			    .has_lastfix = true };

void simulate_no_fix(void)
{
	memset(&gnss_data, 0, sizeof(gnss_data));

	/* Send GNSS data, has fix, but not much more */
	zassert_equal(gnss_update(&gnss_data), 0, "Failed updating GNSS fix");
}

void simulate_fix(void)
{
	memset(&gnss_data, 0, sizeof(gnss_data));
	gnss_data.latest.pvt_flags = 1;
	gnss_data.fix_ok = true;
	gnss_data.lastfix.pvt_flags = 1;
	gnss_data.has_lastfix = true;

	/* Send GNSS data, has fix, but not much more */
	zassert_equal(gnss_update(&gnss_data), 0, "Failed updating GNSS fix");
}

void simulate_easy_fix(void)
{
	simulate_fix();
	gnss_data.latest.pvt_flags = 1;
	gnss_data.latest.num_sv = 7;
	gnss_data.latest.h_dop = 80;
	gnss_data.latest.h_acc_dm = 65;
	gnss_data.latest.height = 100;
	zassert_equal(gnss_update(&gnss_data), 0, "Failed updating GNSS fix");
}

void simulate_accepted_fix(void)
{
	simulate_fix();
	gnss_data.latest.pvt_flags = 1;
	gnss_data.latest.num_sv = 7;
	gnss_data.latest.h_dop = 80;
	gnss_data.latest.h_acc_dm = 25;
	gnss_data.latest.height = 100;
	zassert_equal(gnss_update(&gnss_data), 0, "Failed updating GNSS fix");
}

void amc_gnss_init(void)
{
	k_sem_init(&gnss_fix_timeout, 0, 1);
	zassert_equal(gnss_init(gnss_timeout), 0,
		      "Failed initializing AMC GNSS");
}

void test_gnss_fix(void)
{
	amc_gnss_init();

	/* Send GNSS data, has fix, but not much more */
	simulate_fix();

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
	simulate_easy_fix();

	/* Verify state of easy fix */
	zassert_true(gnss_has_fix(), "Did not have fix");
	zassert_false(gnss_has_accepted_fix(), "Did have accepted fix");
	zassert_true(gnss_has_easy_fix(), "Did not have easy fix");

	/* Accepted fix */
	simulate_accepted_fix();

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
	/* Inactive mode, set first_time_since_start also due to simulate_easy_fix. 
	 * to not receive GNSSMODE_CAUTION.
	 */
	simulate_easy_fix();

	FenceStatus fs = FenceStatus_FenceStatus_Normal;
	CollarStatus cs = CollarStatus_Sleep;
	Mode mode = Mode_Teach;
	amc_zone_t zone = NO_ZONE;

	set_sensor_modes(mode, fs, cs, zone);
	zassert_equal(k_sem_take(&gnss_mode_sem, K_SECONDS(30)), 0, "");
	zassert_equal(current_gnss_mode, GNSSMODE_INACTIVE, "");

	/* Caution mode. */
	simulate_no_fix();

	fs = FenceStatus_FenceStatus_UNKNOWN;
	cs = CollarStatus_CollarStatus_UNKNOWN;
	mode = Mode_Mode_UNKNOWN;
	zone = NO_ZONE;

	set_sensor_modes(mode, fs, cs, zone);
	zassert_equal(k_sem_take(&gnss_mode_sem, K_SECONDS(30)), 0, "");
	zassert_equal(current_gnss_mode, GNSSMODE_CAUTION, "");

	/* PSM mode 1, PSM zone. */
	simulate_easy_fix();

	fs = FenceStatus_FenceStatus_Normal;
	cs = CollarStatus_CollarStatus_Normal;
	mode = Mode_Teach;
	zone = PSM_ZONE;

	set_sensor_modes(mode, fs, cs, zone);
	zassert_equal(k_sem_take(&gnss_mode_sem, K_SECONDS(30)), 0, "");
	zassert_equal(current_gnss_mode, GNSSMODE_PSM, "");

	/* MAX mode. */
	fs = FenceStatus_FenceStatus_Normal;
	cs = CollarStatus_CollarStatus_Normal;
	mode = Mode_Teach;
	zone = WARN_ZONE;

	set_sensor_modes(mode, fs, cs, zone);
	zassert_equal(k_sem_take(&gnss_mode_sem, K_SECONDS(30)), 0, "");
	zassert_equal(current_gnss_mode, GNSSMODE_MAX, "");

	/* PSM mode 2, escaped. */
	simulate_accepted_fix();

	fs = FenceStatus_Escaped;
	cs = CollarStatus_CollarStatus_Normal;
	mode = Mode_Teach;
	zone = NO_ZONE;

	set_sensor_modes(mode, fs, cs, zone);
	zassert_equal(k_sem_take(&gnss_mode_sem, K_SECONDS(30)), 0, "");
	zassert_equal(current_gnss_mode, GNSSMODE_PSM, "");
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