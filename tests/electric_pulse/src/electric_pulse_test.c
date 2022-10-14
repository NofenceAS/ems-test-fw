/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "pwm.h"

#include "ep_module.h"
#include "ep_event.h"
#include "error_event.h"
#include "sound_event.h"

#define PRODUCT_TYPE_SHEEP 1
#define PRODUCT_TYPE_CATTLE 2

static K_SEM_DEFINE(ep_event_sem, 0, 1);

/* Provide custom assert post action handler to handle the assertion on OOM
 * error in Event Manager.
 */
BUILD_ASSERT(!IS_ENABLED(CONFIG_ASSERT_NO_FILE_INFO));

void assert_post_action(const char *file, unsigned int line)
{
	printk("assert_post_action - file: %s (line: %u)\n", file, line);
}

void test_init(void)
{
	zassert_false(event_manager_init(), 
					"Error when initializing event manager");
}

void test_electric_pulse_init(void)
{
	/* Test: Initialization of the EP module! */

	uint16_t product_type;

	/* EP module initialization of sheep collar */
	product_type = PRODUCT_TYPE_SHEEP;
	ztest_returns_value(stg_config_u16_read, 0);
	ztest_return_data(stg_config_u16_read, value, &product_type);
	zassert_equal(ep_module_init(), 0, 
					"EP module initialization returned incorrect value");

	if ((product_type != PRODUCT_TYPE_SHEEP) && 
		(product_type != PRODUCT_TYPE_CATTLE)) {
		product_type = PRODUCT_TYPE_SHEEP;
	}
	zassert_equal(product_type, PRODUCT_TYPE_SHEEP, 
					"EP module initialization provided incorrect product type");

	/* EP module initialization with EEPROM read failure */
	ztest_returns_value(stg_config_u16_read, -1);
	ztest_return_data(stg_config_u16_read, value, &product_type);
	zassert_not_equal(ep_module_init(), 0, 
						"EP module initialization returned incorrect value");

	ztest_returns_value(stg_config_u16_read, 1);
	ztest_return_data(stg_config_u16_read, value, &product_type);
	zassert_not_equal(ep_module_init(), 0, 
						"EP module initialization returned incorrect value");

	/* EP module initialization of cattle collar */
	product_type = PRODUCT_TYPE_CATTLE;
	ztest_returns_value(stg_config_u16_read, 0);
	ztest_return_data(stg_config_u16_read, value, &product_type);
	zassert_equal(ep_module_init(), 0, "Test failed to initializing EP module");

	if ((product_type != PRODUCT_TYPE_SHEEP) && 
		(product_type != PRODUCT_TYPE_CATTLE)) {
		product_type = PRODUCT_TYPE_SHEEP;
	}
	zassert_equal(product_type, PRODUCT_TYPE_CATTLE, 
					"EP module initialization provided incorrect product type");

	/* EP module initialization of unknown collar */
	product_type = UINT16_MAX;
	ztest_returns_value(stg_config_u16_read, 0);
	ztest_return_data(stg_config_u16_read, value, &product_type);
	zassert_equal(ep_module_init(), 0, "Test failed to initializing EP module");

	if ((product_type != PRODUCT_TYPE_SHEEP) && 
		(product_type != PRODUCT_TYPE_CATTLE)) {
		product_type = PRODUCT_TYPE_SHEEP;
	}
	zassert_equal(product_type, PRODUCT_TYPE_SHEEP, 
					"EP module initialization provided incorrect product type");
}

void test_electric_pulse_release_without_warning(void)
{
	/* Test: Submit an electric pulse event without max warning!
	 * A max warning has to be received by the EP module before an electric 
	 * pulse can be triggered. This test should be able to take the error 
	 * semaphore */

	/* Submit an electric pulse event with no prior max warning */
	struct ep_status_event *ep_event = new_ep_status_event();
	ep_event->ep_status = EP_RELEASE;
	EVENT_SUBMIT(ep_event);

	int err = k_sem_take(&ep_event_sem, K_SECONDS(5));
	zassert_equal(err, 0, "Test did not get an error as expected");
}

void test_electric_pulse_release_early(void)
{
	/* Test: Submit a max warning and electric pulse event within safety limit!
	 * An electric pulse can only be triggered at a pre-defined interval.
	 * Electric pulse events before the safety timer expired must be rejected.
	 * This test should be able to take the error semaphore */

	/* Submit a max warning tone event */
	struct sound_status_event *ev_max = new_sound_status_event();
	ev_max->status = SND_STATUS_PLAYING_MAX;
	EVENT_SUBMIT(ev_max);

	/* Submit an electric pulse event before safety timer expires */
	struct ep_status_event *ep_event = new_ep_status_event();
	ep_event->ep_status = EP_RELEASE;
	EVENT_SUBMIT(ep_event);

	int err = k_sem_take(&ep_event_sem, K_SECONDS(5));
	zassert_equal(err, 0, "Test did not get an error as expected");
}

void test_electric_pulse_release_ready(void)
{
	/* Test: Submit a max warning and trigger the electric pulse when ready!
	 * Trigger an electric pulse according to the procedure. This test should
	 * be able to trigger an electric pulse, thus, the error semaphore should
	 * timeout */

	/* Submit a max warning tone event */
	struct sound_status_event *ev_max = new_sound_status_event();
	ev_max->status = SND_STATUS_PLAYING_MAX;
	EVENT_SUBMIT(ev_max);

	/* Wait for safety timer to expired */
	k_sleep(K_SECONDS(6));

	/* Submit an electric pulse event */
	ztest_returns_value(pwm_pin_set_usec, 0);
	ztest_returns_value(pwm_pin_set_usec, 0);
	struct ep_status_event *ready_ep_event = new_ep_status_event();
	ready_ep_event->ep_status = EP_RELEASE;
	EVENT_SUBMIT(ready_ep_event);

	/* If no error is received, the semaphore is not given, 
	 * and it will therefore expire */
	int err = k_sem_take(&ep_event_sem, K_SECONDS(5));
	zassert_equal(err, -EAGAIN, "Test got an unexpected error");
}

void test_electric_pulse_release_snd_wrn(void)
{
	/* Test: Submit an incorrect warning and trigger the electric pulse!
	 * Only a max warning should ready the electric pulse, not any sound
	 * warnings. This test submits a "normal" warning, thus, is test should be 
	 * able to take the error semaphore */

	/* Submit a warning tone event, not a max warning */
	struct sound_status_event *ev_warn = new_sound_status_event();
	ev_warn->status = SND_STATUS_PLAYING_WARN;
	EVENT_SUBMIT(ev_warn);

	/* Submit an electric pulse event */
	struct ep_status_event *ready_ep_event = new_ep_status_event();
	ready_ep_event->ep_status = EP_RELEASE;
	EVENT_SUBMIT(ready_ep_event);

	int err = k_sem_take(&ep_event_sem, K_SECONDS(5));
	zassert_equal(err, 0, "Test did not get an error as expected");
}

void test_main(void)
{
	ztest_test_suite(ep_tests,
			ztest_unit_test(test_init),
			ztest_unit_test(test_electric_pulse_init),
			ztest_unit_test(test_electric_pulse_release_without_warning),
			ztest_unit_test(test_electric_pulse_release_early),
			ztest_unit_test(test_electric_pulse_release_ready),
			ztest_unit_test(test_electric_pulse_release_snd_wrn)
			);
			 
	ztest_run_test_suite(ep_tests);
}

static bool test_event_handler(const struct event_header *eh)
{
	if (is_ep_status_event(eh)) {
		struct ep_status_event *ev = cast_ep_status_event(eh);
		switch (ev->ep_status) {
			case EP_RELEASE: {
				// Do nothing here since we wait for eror event to be returned
				break;
			}
			default: {
				zassert_unreachable("Unexpected command event.");
			}
		}
	}
	if (is_error_event(eh)) {
		struct error_event *ev = cast_error_event(eh);
		switch (ev->sender) {
			case ERR_EP_MODULE: {
				zassert_equal(ev->severity, ERR_SEVERITY_ERROR, 
								"Mismatched severity.");
				zassert_equal(ev->code, -EACCES, 
								"Mismatched error code.");

				// Give sempahore only after error event
				k_sem_give(&ep_event_sem);
				break;
			}
			default: {
				zassert_unreachable("Unexpected command event.");
				break;
			}
		}
	}
	return false;
}

EVENT_LISTENER(test_main, test_event_handler);
EVENT_SUBSCRIBE(test_main, ep_status_event);
EVENT_SUBSCRIBE(test_main, error_event);
