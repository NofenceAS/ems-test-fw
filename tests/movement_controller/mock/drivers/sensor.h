/*
 * Copyright (c) 2022 Nofence AS
 */

#ifndef _SENSOR_H_
#define _SENSOR_H_

#include <zephyr/types.h>
#include <device.h>
#include <errno.h>

#define SENSOR_G		9806650LL
struct sensor_value {
	int32_t val1;
	int32_t val2;
};

enum sensor_channel {
	/** Accelerometer. */
	SENSOR_CHAN_ACCEL_XYZ
};

enum sensor_attribute {
	/** Sensor sampling freq. */
	SENSOR_ATTR_SAMPLING_FREQUENCY,
	SENSOR_ATTR_FULL_SCALE
};
enum sensor_trigger_type { SENSOR_TRIG_DATA_READY };

struct sensor_trigger {
	enum sensor_trigger_type type;
	enum sensor_channel chan;
};

int sensor_sample_fetch(const struct device *dev);
int sensor_channel_get(const struct device *dev, enum sensor_channel chan,
		       struct sensor_value *val);

int sensor_attr_set(const struct device *dev, enum sensor_channel chan,
		    enum sensor_attribute attr, const struct sensor_value *val);

typedef void (*sensor_trigger_handler_t)(const struct device *dev,
					 const struct sensor_trigger *trigger);

int sensor_trigger_set(const struct device *dev,
		       const struct sensor_trigger *trig,
		       sensor_trigger_handler_t handler);

static inline double sensor_value_to_double(const struct sensor_value *val)
{
	return (double)val->val1 + (double)val->val2 / 1000000;
}

static inline void sensor_g_to_ms2(int32_t g, struct sensor_value *ms2)
{
	ms2->val1 = ((int64_t)g * SENSOR_G) / 1000000LL;
	ms2->val2 = ((int64_t)g * SENSOR_G) % 1000000LL;
}

#define movement_sensor spi0

typedef enum {
	TEST_ACTIVITY_NO,
	TEST_ACTIVITY_LOW,
	TEST_ACTIVITY_MED,
	TEST_ACTIVITY_HIGH
} test_id_t;

/* Global variable to keep track of current test */
extern test_id_t curr_test;
/* Global variable to keep track of row in acclelerometer dataset */
extern uint8_t row;

#endif /* _SENSOR_H_ */
