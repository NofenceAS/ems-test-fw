/*
 * Copyright (c) 2018-2019 Peter Bigot Consulting, LLC
 * Copyright (c) 2019-2020 Nordic Semiconductor ASA
 * Copyright (c) 2021 Nofence AS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <zephyr.h>
#include <init.h>
#include <drivers/gpio.h>
#include <drivers/adc.h>
#include <drivers/sensor.h>

#include "battery.h"

#define MODULE battery
#include <logging/log.h>

LOG_MODULE_REGISTER(MODULE, CONFIG_BATTERY_LOG_LEVEL);

#define VBATT DT_PATH(vbatt)

/* Use divider that reduce battery voltage to
 * the maximum supported by the hardware (3.6 V)
 */
#define BATTERY_ADC_GAIN ADC_GAIN_1_6

struct io_channel_config {
	const char *label;
	uint8_t channel;
};

struct gpio_channel_config {
	const char *label;
	uint8_t pin;
	uint8_t flags;
};

struct divider_config {
	struct io_channel_config io_channel;
	struct gpio_channel_config power_gpios;
	/* output_ohm is used as a flag value: if it is nonzero then
	 * the battery is measured through a voltage divider;
	 * otherwise it is assumed to be directly connected to Vdd.
	 */
	uint32_t output_ohm;
	uint32_t full_ohm;
};

static const struct divider_config divider_config = {
#if DT_NODE_HAS_STATUS(VBATT, okay)
	.io_channel = {
		DT_IO_CHANNELS_LABEL(VBATT),
		DT_IO_CHANNELS_INPUT(VBATT),
	},
#if DT_NODE_HAS_PROP(VBATT, power_gpios)
	.power_gpios = {
		DT_GPIO_LABEL(VBATT, power_gpios),
		DT_GPIO_PIN(VBATT, power_gpios),
		DT_GPIO_FLAGS(VBATT, power_gpios),
	},
#endif
	.output_ohm = DT_PROP(VBATT, output_ohms),
	.full_ohm = DT_PROP(VBATT, full_ohms),
#else /* /vbatt exists */
	.io_channel = {
		DT_LABEL(DT_ALIAS(adc_0)),
	},
#endif /* /vbatt exists */
};

struct divider_data {
	const struct device *adc;
	const struct device *gpio;
	struct adc_channel_cfg adc_cfg;
	struct adc_sequence adc_seq;
	int16_t raw;
};
static struct divider_data divider_data;

static int divider_setup(void)
{
	const struct divider_config *cfg = &divider_config;
	const struct io_channel_config *iocp = &cfg->io_channel;
	const struct gpio_channel_config *gcp = &cfg->power_gpios;
	struct divider_data *ddp = &divider_data;
	struct adc_sequence *asp = &ddp->adc_seq;
	struct adc_channel_cfg *accp = &ddp->adc_cfg;
	int err;

	if (iocp->label == NULL) {
		return -ENOTSUP;
	}

	ddp->adc = device_get_binding(iocp->label);
	if (ddp->adc == NULL) {
		LOG_ERR("Failed to get ADC %s", iocp->label);
		return -ENOENT;
	}

	if (gcp->label) {
		ddp->gpio = device_get_binding(gcp->label);
		if (ddp->gpio == NULL) {
			LOG_ERR("Failed to get GPIO %s", gcp->label);
			return -ENOENT;
		}
		err = gpio_pin_configure(ddp->gpio, gcp->pin,
					 GPIO_OUTPUT_INACTIVE | gcp->flags);
		if (err != 0) {
			LOG_ERR("Failed to control feed %s.%u: %d", gcp->label,
				gcp->pin, err);
			return err;
		}
	}

	*asp = (struct adc_sequence){
		.channels = BIT(0),
		.buffer = &ddp->raw,
		.buffer_size = sizeof(ddp->raw),
		.oversampling = 4,
		.calibrate = true,
	};

#ifdef CONFIG_ADC_NRFX_SAADC
	*accp = (struct adc_channel_cfg){
		.gain = BATTERY_ADC_GAIN,
		.reference = ADC_REF_INTERNAL,
		.acquisition_time = ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 40),
	};

	if (cfg->output_ohm != 0) {
		accp->input_positive =
			SAADC_CH_PSELP_PSELP_AnalogInput0 + iocp->channel;
	} else {
		accp->input_positive = SAADC_CH_PSELP_PSELP_VDD;
	}

	asp->resolution = 14;
#else /* For native posix unit test */
	*accp = (struct adc_channel_cfg){
		.gain = BATTERY_ADC_GAIN,
		.reference = ADC_REF_INTERNAL,
		.acquisition_time = ADC_ACQ_TIME_DEFAULT,
	};
	asp->resolution = 10;

#endif /* CONFIG_ADC_var */

	err = adc_channel_setup(ddp->adc, accp);
	LOG_INF("Setup battery sense on AIN_%u", iocp->channel);

	return err;
}

static bool battery_ok;

static int battery_setup(const struct device *arg)
{
	int err = divider_setup();

	battery_ok = (err == 0);
	LOG_INF("Battery divider setup %s", battery_ok ? "complete" : "error");
	return err;
}

int battery_measure_enable(bool enable)
{
	int err = -ENOENT;

	if (battery_ok) {
		const struct divider_data *ddp = &divider_data;
		const struct gpio_channel_config *gcp =
			&divider_config.power_gpios;

		err = 0;
		if (ddp->gpio) {
			err = gpio_pin_set(ddp->gpio, gcp->pin, enable);
		}
	}
	return err;
}

int battery_sample(void)
{
	int rc = -ENOENT;

	if (battery_ok) {
		struct divider_data *ddp = &divider_data;
		const struct divider_config *dcp = &divider_config;
		struct adc_sequence *sp = &ddp->adc_seq;

		rc = adc_read(ddp->adc, sp);
		sp->calibrate = false;
		if (rc == 0) {
			int32_t val = ddp->raw;

			adc_raw_to_millivolts(adc_ref_internal(ddp->adc),
					      ddp->adc_cfg.gain, sp->resolution,
					      &val);

			if (dcp->output_ohm != 0) {
				rc = val * (uint64_t)dcp->full_ohm /
				     dcp->output_ohm;

			} else {
				rc = val;
			}
		}
	}
	/* Return battery voltage measured in mV */
	return rc;
}

unsigned int battery_level_soc(unsigned int batt_mV,
			       const struct battery_level_point *curve)
{
	const struct battery_level_point *pb = curve;

	if (batt_mV >= pb->lvl_mV) {
		/* Measured voltage above highest point, cap at maximum. */
		return (pb->lvl_pptt / 100);
	}
	/* Go down to the last point at or below the measured voltage. */
	while ((pb->lvl_pptt > 0) && (batt_mV < pb->lvl_mV)) {
		++pb;
	}
	if (batt_mV < pb->lvl_mV) {
		/* Below lowest point, cap at minimum */
		return pb->lvl_pptt;
	}

	/* Linear interpolation between below and above points. */
	const struct battery_level_point *pa = pb - 1;
	int batt_level_pptt = pb->lvl_pptt + ((pa->lvl_pptt - pb->lvl_pptt) *
					      (batt_mV - pb->lvl_mV) /
					      (pa->lvl_mV - pb->lvl_mV));

	/* Return the battery State Of Charge (SOC) in % based on battery discharge curve */
	unsigned int batt_level_precentage = (batt_level_pptt / 100);
	return batt_level_precentage;
}

/* Initialze battery setup on startup */
SYS_INIT(battery_setup, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);