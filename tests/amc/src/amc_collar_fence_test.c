#include "amc_test_common.h"
#include "amc_gnss.h"
#include <ztest.h>
#include "amc_states_cache.h"
#include "embedded.pb.h"
#include "gnss_controller_events.h"
#include "messaging_module_events.h"

K_SEM_DEFINE(error_handler_sem, 0, 1);
K_SEM_DEFINE(collar_status_sem, 0, 1);
static CollarStatus current_collar_status = CollarStatus_CollarStatus_UNKNOWN;

void test_collar_status(void)
{
	/* Unknown -> normal. */
	ztest_returns_value(eep_uint8_write, 0);
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
}

static bool event_handler(const struct event_header *eh)
{
	if (is_update_collar_status(eh)) {
		struct update_collar_status *ev = cast_update_collar_status(eh);
		current_collar_status = ev->collar_status;
		k_sem_give(&collar_status_sem);
	}
	return false;
}

EVENT_LISTENER(test_collar_fence_handler, event_handler);
EVENT_SUBSCRIBE(test_collar_fence_handler, update_collar_status);