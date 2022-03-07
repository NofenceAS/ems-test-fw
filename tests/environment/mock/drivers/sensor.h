/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _SENSOR_H_
#define _SENSOR_H_

#include <zephyr/types.h>
#include <device.h>
#include <errno.h>

struct sensor_value {
	int32_t val1;
	int32_t val2;
};

enum sensor_channel {
	/** Pressure in kilopascal. */
	SENSOR_CHAN_PRESS,
	/** Humidity, in percent. */
	SENSOR_CHAN_HUMIDITY,
	/** Ambient temperature in degrees Celsius. */
	SENSOR_CHAN_AMBIENT_TEMP
};

int sensor_sample_fetch(const struct device *dev);
int sensor_channel_get(const struct device *dev, enum sensor_channel chan,
		       struct sensor_value *val);

#define environment_sensor spi0

typedef enum {
	TEST_SANITY_PASS,
	TEST_SANITY_FAIL_TEMP,
	TEST_SANITY_FAIL_PRESS,
	TEST_SANITY_FAIL_HUMIDITY,
	TEST_SANITY_FAIL_TEMP_WARN,
	TEST_SANITY_FAIL_PRESS_WARN,
	TEST_SANITY_FAIL_HUMIDITY_WARN
} test_id_t;

static inline double sensor_value_to_double(const struct sensor_value *val)
{
	return (double)val->val1 + (double)val->val2 / 1000000;
}

extern test_id_t cur_test;

#endif /* _SENSOR_H_ */
