#include <drivers/sensor.h>
#include <ztest.h>
#include <zephyr.h>

int sensor_sample_fetch(const struct device *dev)
{
	ARG_UNUSED(dev);
	return ztest_get_return_value();
}

void simulate_temp_values(struct sensor_value *val)
{
	if (cur_test == TEST_SANITY_FAIL_TEMP) {
		val->val1 = -200;
		val->val2 = -62809;
	} else if (cur_test == TEST_SANITY_FAIL_TEMP_WARN) {
		val->val1 = 150;
		val->val2 = 34564;
	} else {
		/* All other cases outputs a valid temp. */
		val->val1 = 25;
		val->val2 = 13532;
	}
}

void simulate_press_values(struct sensor_value *val)
{
	if (cur_test == TEST_SANITY_FAIL_PRESS) {
		val->val1 = 300;
		val->val2 = 35445;
	} else if (cur_test == TEST_SANITY_FAIL_PRESS_WARN) {
		val->val1 = 150;
		val->val2 = 6345;
	} else {
		/* All other cases outputs a valid pressure. */
		val->val1 = 95;
		val->val2 = 125;
	}
}

void simulate_humidity_values(struct sensor_value *val)
{
	if (cur_test == TEST_SANITY_FAIL_HUMIDITY) {
		val->val1 = 150000;
		val->val2 = 23452;
	} else if (cur_test == TEST_SANITY_FAIL_HUMIDITY_WARN) {
		val->val1 = -5;
		val->val2 = -34534;
	} else {
		/* All other cases outputs a valid humidity. */
		val->val1 = 11;
		val->val2 = 100532;
	}
}

int sensor_channel_get(const struct device *dev, enum sensor_channel chan, struct sensor_value *val)
{
	ARG_UNUSED(dev);
	switch (chan) {
	case SENSOR_CHAN_AMBIENT_TEMP:
		simulate_temp_values(val);
		break;
	case SENSOR_CHAN_PRESS:
		simulate_press_values(val);
		break;
	case SENSOR_CHAN_HUMIDITY:
		simulate_humidity_values(val);
		break;
	default:
		zassert_unreachable("");
		break;
	}
	return ztest_get_return_value();
}