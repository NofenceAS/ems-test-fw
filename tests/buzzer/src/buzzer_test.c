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

void test_init_event_manager(void)
{
	zassert_false(event_manager_init(),
		      "Error when initializing event manager");
}
void test_init_buzzer(void)
{
	ztest_returns_value(pwm_get_cycles_per_sec, 0);
	//ztest_returns_value(pwm_pin_set_usec, 0);
	zassert_false(buzzer_module_init(), "Error when initializing buzzer");
}

void test_main(void)
{
	ztest_test_suite(buzzer_tests, ztest_unit_test(test_init_event_manager),
			 ztest_unit_test(test_init_buzzer));

	ztest_run_test_suite(buzzer_tests);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_sound_event(eh)) {
		return false;
	}
	zassert_true(false, "Unexpected event type received");
	return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, sound_event);
