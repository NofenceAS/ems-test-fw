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

#endif /* _SENSOR_H_ */
