/*
 * Copyright (c) 2022 Nofence AS
 */

#include <zephyr.h>
#include "env_sensor.h"
#include "env_sensor_event.h"
#include <logging/log.h>
#include <drivers/sensor.h>

#define LOG_MODULE_NAME env_sensor
LOG_MODULE_REGISTER(LOG_MODULE_NAME, CONFIG_ENV_SENSOR_LOG_LEVEL);

static inline int update_env_sensor_event_values(void)
{
	const struct device *bme_dev =
		device_get_binding(DT_LABEL(DT_NODELABEL(environment_sensor)));

	if (bme_dev == NULL) {
		LOG_ERR("BME280 - No device found");
		return -ENODEV;
	}

	struct sensor_value temp, press, humidity;

	int err = sensor_sample_fetch(bme_dev);
	if (err) {
		LOG_ERR("Error fetching BME sensor values, err %d", err);
		return err;
	}

	err = sensor_channel_get(bme_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
	if (err) {
		LOG_ERR("Error fetching TEMP BME value, err %d", err);
		return err;
	}

	err = sensor_channel_get(bme_dev, SENSOR_CHAN_PRESS, &press);
	if (err) {
		LOG_ERR("Error fetching PRESSURE BME value, err %d", err);
		return err;
	}

	err = sensor_channel_get(bme_dev, SENSOR_CHAN_HUMIDITY, &humidity);
	if (err) {
		LOG_ERR("Error fetching HUMIDITY BME value, err %d", err);
		return err;
	}

	struct env_sensor_event *ev = new_env_sensor_event();

	ev->temp_integer = temp.val1;
	ev->temp_integer = temp.val2;

	ev->press_integer = press.val1;
	ev->press_integer = press.val2;

	ev->humidity_integer = humidity.val1;
	ev->humidity_integer = humidity.val2;

	EVENT_SUBMIT(ev);

	return 0;
}

/**
 * @brief Main event handler function. 
 * 
 * @param[in] eh Event_header for the if-chain to 
 *               use to recognize which event triggered.
 * 
 * @return True or false based on if we want to consume the event or not.
 */
static bool event_handler(const struct event_header *eh)
{
	//int err;
	if (is_request_env_sensor_event(eh)) {
		update_env_sensor_event_values();
		return true;
	}
	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}

EVENT_LISTENER(LOG_MODULE_NAME, event_handler);
EVENT_SUBSCRIBE(LOG_MODULE_NAME, request_env_sensor_event);