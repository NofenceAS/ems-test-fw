#include <drivers/sensor.h>
#include <ztest.h>
#include <zephyr.h>

int sensor_sample_fetch(const struct device *dev)
{
	ARG_UNUSED(dev);
	return ztest_get_return_value();
}

void simulate_sensor_values(enum sensor_channel chan, struct sensor_value *val)
{
	if (cur_test == TEST_SANITY_PASS) {
		switch (chan) {
		case SENSOR_CHAN_AMBIENT_TEMP:
			val->val1 = 25;
			val->val2 = 13532;
			break;
		case SENSOR_CHAN_PRESS:
			val->val1 = 95;
			val->val2 = 125;
			break;
		case SENSOR_CHAN_HUMIDITY:
			val->val1 = 11;
			val->val2 = 100532;
			break;
		default:
			break;
		}
	} else if (cur_test == TEST_SANITY_FAIL_TEMP) {
		switch (chan) {
		case SENSOR_CHAN_AMBIENT_TEMP:
			val->val1 = -7653;
			val->val2 = 281552;
			break;
		case SENSOR_CHAN_PRESS:
			val->val1 = 96;
			val->val2 = 22851;
			break;
		case SENSOR_CHAN_HUMIDITY:
			val->val1 = 15;
			val->val2 = 203158;
			break;
		default:
			break;
		}
	} else if (cur_test == TEST_SANITY_FAIL_PRESSURE) {
		switch (chan) {
		case SENSOR_CHAN_AMBIENT_TEMP:
			val->val1 = 25;
			val->val2 = 215562;
			break;
		case SENSOR_CHAN_PRESS:
			val->val1 = 150;
			val->val2 = 226510;
			break;
		case SENSOR_CHAN_HUMIDITY:
			val->val1 = 15;
			val->val2 = 690315;
			break;
		default:
			break;
		}
	} else if (cur_test == TEST_SANITY_FAIL_HUMIDITY) {
		switch (chan) {
		case SENSOR_CHAN_AMBIENT_TEMP:
			val->val1 = 25;
			val->val2 = 721552;
			break;
		case SENSOR_CHAN_PRESS:
			val->val1 = 96;
			val->val2 = 347510;
			break;
		case SENSOR_CHAN_HUMIDITY:
			val->val1 = -5;
			val->val2 = 677315;
			break;
		default:
			break;
		}
	}
}

int sensor_channel_get(const struct device *dev, enum sensor_channel chan,
		       struct sensor_value *val)
{
	ARG_UNUSED(dev);
	simulate_sensor_values(chan, val);
	return ztest_get_return_value();
}