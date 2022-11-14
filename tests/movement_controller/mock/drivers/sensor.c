#include <drivers/sensor.h>
#include <ztest.h>
#include <zephyr.h>

#include "acc_dataset.h"

void simulate_acc_values(struct sensor_value *val)
{
	if (curr_test == TEST_ACTIVITY_NO) {	
		/* No movement */
		val[0].val1 = acc_dataset_no_mov.rows[row].x_val1;
		val[0].val2 = acc_dataset_no_mov.rows[row].x_val2;
		val[1].val1 = acc_dataset_no_mov.rows[row].y_val1;
		val[1].val2 = acc_dataset_no_mov.rows[row].y_val2;
		val[2].val1 = acc_dataset_no_mov.rows[row].z_val1;
		val[2].val2 = acc_dataset_no_mov.rows[row].z_val2;
	} else if(curr_test == TEST_ACTIVITY_LOW) {
		/* Resting or grazing */
		val[0].val1 = acc_dataset_resting.rows[row].x_val1;
		val[0].val2 = acc_dataset_resting.rows[row].x_val2;
		val[1].val1 = acc_dataset_resting.rows[row].y_val1;
		val[1].val2 = acc_dataset_resting.rows[row].y_val2;
		val[2].val1 = acc_dataset_resting.rows[row].z_val1;
		val[2].val2 = acc_dataset_resting.rows[row].z_val2;

	} else if (curr_test == TEST_ACTIVITY_MED) {
		/* Walking */
		val[0].val1 = acc_dataset_walking.rows[row].x_val1;
		val[0].val2 = acc_dataset_walking.rows[row].x_val2;
		val[1].val1 = acc_dataset_walking.rows[row].y_val1;
		val[1].val2 = acc_dataset_walking.rows[row].y_val2;
		val[2].val1 = acc_dataset_walking.rows[row].z_val1;
		val[2].val2 = acc_dataset_walking.rows[row].z_val2;
		
	} else if (curr_test == TEST_ACTIVITY_HIGH) {
		/* Running */
		val[0].val1 = acc_dataset_running.rows[row].x_val1;
		val[0].val2 = acc_dataset_running.rows[row].x_val2;
		val[1].val1 = acc_dataset_running.rows[row].y_val1;
		val[1].val2 = acc_dataset_running.rows[row].y_val2;
		val[2].val1 = acc_dataset_running.rows[row].z_val1;
		val[2].val2 = acc_dataset_running.rows[row].z_val2;
	}
}


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
	simulate_acc_values(val);
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