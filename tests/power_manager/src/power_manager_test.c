/*
 * Copyright (c) 2022 Nofence AS
 */

#include <drivers/adc.h>
#include <drivers/adc/adc_emul.h>
#include <zephyr.h>
#include <ztest.h>
#include "pwr_event.h"
#include "pwr_module.h"
#include "error_event.h"

#define ADC_DEVICE_NAME DT_LABEL(DT_INST(0, zephyr_adc_emul))
#define ADC_REF_INTERNAL_MV                                                    \
	DT_PROP(DT_INST(0, zephyr_adc_emul), ref_internal_mv)
#define ADC_REF_EXTERNAL1_MV                                                   \
	DT_PROP(DT_INST(0, zephyr_adc_emul), ref_external1_mv)
#define ADC_RESOLUTION 14
#define ADC_ACQUISITION_TIME ADC_ACQ_TIME_DEFAULT
#define ADC_1ST_CHANNEL_ID 0
#define ADC_2ND_CHANNEL_ID 1

static K_SEM_DEFINE(normal_pwr_event_sem, 0, 1);
static K_SEM_DEFINE(low_pwr_event_sem, 0, 1);
static K_SEM_DEFINE(critical_pwr_event_sem, 0, 1);

#define VOLTAGE_OFFSET_MV 100
/* Provide custom assert post action handler to handle the assertion on OOM
 * error in Event Manager.
 */
BUILD_ASSERT(!IS_ENABLED(CONFIG_ASSERT_NO_FILE_INFO));

const struct device *get_adc_device(void)
{
	const struct device *adc_dev = device_get_binding(ADC_DEVICE_NAME);

	zassert_not_null(adc_dev, "Cannot get ADC device");

	return adc_dev;
}

void assert_post_action(const char *file, unsigned int line)
{
	printk("assert_post_action - file: %s (line: %u)\n", file, line);
}

void test_init(void)
{
	zassert_false(event_manager_init(),
		      "Error when initializing event manager");
}

void test_pwr_module_init(void)
{
	/* Test with a givevn voltage */
	const uint16_t input_mv = 3800;
	/* Generic ADC setup */
	const struct device *adc_dev = get_adc_device();

	/* ADC emulator-specific setup */
	int ret =
		adc_emul_const_value_set(adc_dev, ADC_1ST_CHANNEL_ID, input_mv);
	zassert_ok(ret, "adc_emul_const_value_set() failed with code %d", ret);

	zassert_equal(pwr_module_init(), 0,
		      "Error when initializing pwr module");

	int err = k_sem_take(&normal_pwr_event_sem, K_SECONDS(5));
	zassert_equal(err, 0, "normal_pwr_event_sem hanged.");
}

void test_pwr_module_low(void)
{
	const uint16_t input_mv = CONFIG_BATTERY_LOW - VOLTAGE_OFFSET_MV;
	/* Generic ADC setup */
	const struct device *adc_dev = get_adc_device();

	/* ADC emulator-specific setup */
	int ret =
		adc_emul_const_value_set(adc_dev, ADC_1ST_CHANNEL_ID, input_mv);
	zassert_ok(ret, "adc_emul_const_value_set() failed with code %d", ret);

	int err = k_sem_take(&low_pwr_event_sem, K_SECONDS(5));
	zassert_equal(err, 0, "low_pwr_event_sem hanged.");

	/* Check stuff when event is received here */
}

void test_pwr_module_critical(void)
{
	const uint16_t input_mv = CONFIG_BATTERY_CRITICAL - VOLTAGE_OFFSET_MV;
	/* Generic ADC setup */
	const struct device *adc_dev = get_adc_device();

	/* ADC emulator-specific setup */
	int ret =
		adc_emul_const_value_set(adc_dev, ADC_1ST_CHANNEL_ID, input_mv);
	zassert_ok(ret, "adc_emul_const_value_set() failed with code %d", ret);

	int err = k_sem_take(&critical_pwr_event_sem, K_SECONDS(5));
	zassert_equal(err, 0, "critical_pwr_event_sem hanged.");

	/* Check stuff when event is received here */
}

void test_main(void)
{
	ztest_test_suite(pwr_tests, ztest_unit_test(test_init),
			 ztest_unit_test(test_pwr_module_init),
			 ztest_unit_test(test_pwr_module_low),
			 ztest_unit_test(test_pwr_module_critical));
	ztest_run_test_suite(pwr_tests);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_pwr_status_event(eh)) {
		struct pwr_status_event *ev = cast_pwr_status_event(eh);
		switch (ev->pwr_state) {
		case PWR_NORMAL:
			printk("PWR NORMAL\n");
			/* Do nothing here */
			k_sem_give(&normal_pwr_event_sem);
			break;
		case PWR_LOW:
			printk("PWR LOW\n");
			/* Do nothing here */
			k_sem_give(&low_pwr_event_sem);
			break;
		case PWR_CRITICAL:
			printk("PWR CRTICAL\n");
			k_sem_give(&critical_pwr_event_sem);
			/* Do nothing here */
			break;
		case PWR_BATTERY:
			/* This state is periodic sent based on CONFIG_BATTRY_POLLER_WORK_SEC 
			   Can be used to fetch battery volatge 
			*/
			break;
		default:
			zassert_unreachable("Unexpected command event.");
			break;
		}
	}
	if (is_error_event(eh)) {
		struct error_event *ev = cast_error_event(eh);
		switch (ev->sender) {
		case ERR_PWR_MODULE:
			zassert_equal(ev->code, -EACCES,
				      "Mismatched error code.");
			break;

		default:
			zassert_unreachable("Unexpected command event.");
			break;
		}
	}
	return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, pwr_status_event);
EVENT_SUBSCRIBE(test_main, error_event);
