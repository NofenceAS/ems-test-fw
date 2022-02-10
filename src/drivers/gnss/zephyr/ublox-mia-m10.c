
#define DT_DRV_COMPAT u_blox_mia_m10

#include <init.h>
#include <drivers/gpio.h>
#include <drivers/uart.h>
#include <logging/log.h>

#include "gnss.h"
#include "ublox-mia-m10.h"

LOG_MODULE_REGISTER(MIA_M10, CONFIG_GNSS_LOG_LEVEL);

/**
 *
 */

static int mia_m10_position_fetch(const struct device *dev)
{
	return 0;
}

static const struct gnss_driver_api mia_m10_api_funcs = {
	.position_fetch = mia_m10_position_fetch,
};

static int mia_m10_init(const struct device *dev)
{
	return 0;
}

static struct mia_m10_dev_data mia_m10_data;

static const struct mia_m10_dev_config mia_m10_config = {
	.uart_name = DT_INST_BUS_LABEL(0),
};

DEVICE_DT_INST_DEFINE(0, mia_m10_init, NULL,
		    &mia_m10_data, &mia_m10_config, POST_KERNEL,
		    CONFIG_GNSS_INIT_PRIORITY, &mia_m10_api_funcs);
