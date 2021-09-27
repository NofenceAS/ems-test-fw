/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file Sample app to demonstrate PWM.
 */

#include "ble/nf_ble.h"
#include <autoconf.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gps.h>
#include <drivers/sensor.h>
#include <drivers/flash.h>
#include <logging/log.h>
#include <stdio.h>
#include <sys/printk.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(main);

//#define TEST_ZOEM8B_GPS 1
// #define TEST_LIS2DH 1
// #define TEST_FLASH 1

#define GSM_DEVICE DT_LABEL(DT_INST(0, ublox_sara_r4))
#define FLASH_DEVICE DT_LABEL(DT_INST(0, jedec_spi_nor))
#define FLASH_NAME "JEDEC SPI-NOR"
#define FLASH_TEST_REGION_OFFSET 0x000
#define FLASH_PAGE_SIZE 256

#ifdef TEST_LIS2DH
static void fetch_and_display(const struct device *sensor)
{
	static unsigned int count;
	struct sensor_value accel[3];
	const char *overrun = "";
	int rc = sensor_sample_fetch(sensor);

	++count;
	if (rc == -EBADMSG) {
		/* Sample overrun.  Ignore in polled mode. */
		if (IS_ENABLED(CONFIG_LIS2DH_TRIGGER)) {
			overrun = "[OVERRUN] ";
		}
		rc = 0;
	}
	if (rc == 0) {
		rc = sensor_channel_get(sensor, SENSOR_CHAN_ACCEL_XYZ, accel);
	}
	if (rc < 0) {
		printk("ERROR: Update failed: %d\n", rc);
	} else {
		printf("#%u @ %u ms: %sx %f , y %f , z %f\n", count, k_uptime_get_32(), overrun,
		       sensor_value_to_double(&accel[0]), sensor_value_to_double(&accel[1]),
		       sensor_value_to_double(&accel[2]));
	}
}

#ifdef CONFIG_LIS2DH_TRIGGER
static void trigger_handler(const struct device *dev, struct sensor_trigger *trig)
{
	fetch_and_display(dev);
}
#endif
#endif

#ifdef TEST_ZOEM8B_GPS
static void gps_event_handler(const struct device *dev, struct gps_event *evt)
{
}

/* GPS device. Used to identify the GPS driver in the sensor API. */
static const struct device *gps_dev;

static int setup_gps(void)
{
	int err;

	gps_dev = device_get_binding(DT_LABEL(DT_INST(0, ublox_zoem8bspi)));
	if (gps_dev == NULL) {
		LOG_ERR("Could not get %s device", CONFIG_ZOEM8B_GPS_DEV_NAME);
		return -ENODEV;
	}

	err = gps_init(gps_dev, gps_event_handler);
	if (err) {
		LOG_ERR("Could not initialize GPS, error: %d", err);
		return err;
	}

	return 0;
}
#endif

void main(void)
{
	// sys_pm_ctrl_disable_state(SYS_POWER_STATE_DEEP_SLEEP_1);
	printk("main %p\n", k_current_get());
	LOG_ERR("main starting!\n");
#if defined(TEST_SCAN)
	nf_ble_init();
#endif
	const struct device *gsm_dev = device_get_binding(GSM_DEVICE);
	if (!gsm_dev) {
		LOG_ERR("GSM driver %s was not found!\n", GSM_DEVICE);
		return;
	}
#if defined(TEST_FLASH)
	const struct device *flash_dev;
	const uint8_t expected[] = { 0x55, 0xaa, 0x66, 0x99 };
	const size_t len = sizeof(expected);
	uint8_t buf[sizeof(expected)];
	int rc;

	LOG_INF("\n" FLASH_NAME " SPI flash testing\n");
	LOG_INF("==========================\n");

	flash_dev = device_get_binding(FLASH_DEVICE);

	if (!flash_dev) {
		LOG_ERR("SPI flash driver %s was not found!\n", FLASH_DEVICE);
		return;
	}

	LOG_INF("\nTest 1: Flash erase\n");

	rc = flash_erase(flash_dev, FLASH_TEST_REGION_OFFSET, FLASH_PAGE_SIZE);
	if (rc != 0) {
		LOG_ERR("Flash erase failed! %d\n", rc);
	} else {
		LOG_INF("Flash erase succeeded!\n");
	}

	LOG_INF("\nTest 2: Flash write\n");

	LOG_INF("Attempting to write %u bytes\n", len);
	rc = flash_write(flash_dev, FLASH_TEST_REGION_OFFSET, expected, len);
	if (rc != 0) {
		LOG_ERR("Flash write failed! %d\n", rc);
		return;
	}

	memset(buf, 0, len);
	rc = flash_read(flash_dev, FLASH_TEST_REGION_OFFSET, buf, len);
	if (rc != 0) {
		LOG_ERR("Flash read failed! %d\n", rc);
		return;
	}

	if (memcmp(expected, buf, len) == 0) {
		LOG_INF("Data read matches data written. Good!!\n");
	} else {
		const uint8_t *wp = expected;
		const uint8_t *rp = buf;
		const uint8_t *rpe = rp + len;

		printf("Data read does not match data written!!\n");
		while (rp < rpe) {
			LOG_ERR("%08x wrote %02x read %02x %s\n",
				(uint32_t)(FLASH_TEST_REGION_OFFSET + (rp - buf)), *wp, *rp,
				(*rp == *wp) ? "match" : "MISMATCH");
			++rp;
			++wp;
		}
	}

#endif

#ifdef TEST_ZOEM8B_GPS
	setup_gps();
#endif

#if defined(TEST_SCAN)
	nf_ble_start_scan();
#endif
#ifdef TEST_LIS2DH
	const struct device *sensor = device_get_binding(DT_LABEL(DT_INST(0, st_lis2dh)));

	if (sensor == NULL) {
		printk("No device found\n");
	}
	if (!device_is_ready(sensor)) {
		printk("Device %s is not ready\n", sensor->name);
	} else {
		printk("Device %s is ready :-)\n", sensor->name);
	}

#if CONFIG_LIS2DH_TRIGGER
	{
		struct sensor_trigger trig;
		int rc;

		trig.type = SENSOR_TRIG_DATA_READY;
		trig.chan = SENSOR_CHAN_ACCEL_XYZ;

		if (IS_ENABLED(CONFIG_LIS2DH_ODR_RUNTIME)) {
			struct sensor_value odr = {
				.val1 = 1,
			};

			rc = sensor_attr_set(sensor, trig.chan, SENSOR_ATTR_SAMPLING_FREQUENCY,
					     &odr);
			if (rc != 0) {
				printf("Failed to set odr: %d\n", rc);
				return;
			}
			printf("Sampling at %u Hz\n", odr.val1);
		}

		rc = sensor_trigger_set(sensor, &trig, trigger_handler);
		if (rc != 0) {
			printf("Failed to set trigger: %d\n", rc);
			return;
		}

		printf("Waiting for triggers\n");
		while (true) {
			k_sleep(K_MSEC(2000));
		}
	}
#else /* CONFIG_LIS2DH_TRIGGER */
	printf("Polling at 0.5 Hz\n");
	while (true) {
		fetch_and_display(sensor);
		k_sleep(K_MSEC(2000));
	}
#endif /* CONFIG_LIS2DH_TRIGGER */
#endif /* TEST_LIS2DH */

#ifdef TEST_ZOEM8B_GPS
	struct gps_config gps_cfg = {
		.nav_mode = GPS_NAV_MODE_PERIODIC,
		.power_mode = GPS_POWER_MODE_DISABLED,
		.timeout = 360,
		.interval = 360,
		.priority = true,
	};
	gps_start(gps_dev, &gps_cfg);
#endif
}