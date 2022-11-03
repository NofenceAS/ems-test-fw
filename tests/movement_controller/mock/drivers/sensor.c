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
	zassert_equal(chan, SENSOR_CHAN_ACCEL_XYZ, "");
	return ztest_get_return_value();
}

int sensor_attr_set(const struct device *dev, enum sensor_channel chan,
		    enum sensor_attribute attr, const struct sensor_value *val)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(val);

	zassert_equal(chan, SENSOR_CHAN_ACCEL_XYZ, "");

	return ztest_get_return_value();
}

int sensor_trigger_set(const struct device *dev,
		       const struct sensor_trigger *trig,
		       sensor_trigger_handler_t handler)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(handler);

	zassert_equal(trig->type, SENSOR_TRIG_DATA_READY, "");
	zassert_equal(trig->chan, SENSOR_CHAN_ACCEL_XYZ, "");

	return ztest_get_return_value();
}