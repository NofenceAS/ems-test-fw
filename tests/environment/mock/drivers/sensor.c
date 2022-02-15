#include <drivers/sensor.h>
#include <ztest.h>
#include <zephyr.h>

int sensor_sample_fetch(const struct device *dev)
{
	ARG_UNUSED(dev);
	return ztest_get_return_value();
}

int sensor_channel_get(const struct device *dev, enum sensor_channel chan,
		       struct sensor_value *val)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(chan);
	val->val1 = 12;
	val->val2 = 13000;
	return ztest_get_return_value();
}