
#ifndef UBLOX_MIA_M10_H_
#define UBLOX_MIA_M10_H_

#include <zephyr/types.h>
#include <device.h>
#include <drivers/gpio.h>
#include <drivers/uart.h>

struct mia_m10_dev_data {
	const struct device *uart;

	unsigned int utc_time;

	double lat;
	double lon;
};

struct mia_m10_dev_config {
	const char *uart_name;
};

int mia_m10_config_get(uint32_t key, uint8_t size, uint64_t* raw_value);

#endif /* UBLOX_MIA_M10_H_ */