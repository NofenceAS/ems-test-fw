
#ifndef UBLOX_MIA_M10_H_
#define UBLOX_MIA_M10_H_

#include <zephyr/types.h>
#include <device.h>
#include <drivers/gpio.h>
#include <drivers/uart.h>

#define MIA_M10_DEFAULT_BAUDRATE	38400

struct mia_m10_dev_data {
	const struct device *uart;

	unsigned int utc_time;

	double lat;
	double lon;
};

struct mia_m10_dev_config {
	const char *uart_name;
};

int mia_m10_config_get_u8(uint32_t key, uint8_t* value);
int mia_m10_config_get_u16(uint32_t key, uint16_t* value);
int mia_m10_config_get_u32(uint32_t key, uint32_t* value);
int mia_m10_config_get_f64(uint32_t key, double* value);
int mia_m10_config_get(uint32_t key, uint8_t size, uint64_t* raw_value);

int mia_m10_config_set_u8(uint32_t key, uint8_t value);
int mia_m10_config_set_u16(uint32_t key, uint16_t value);
int mia_m10_config_set_u32(uint32_t key, uint32_t value);
int mia_m10_config_set(uint32_t key, uint64_t raw_value);

int mia_m10_send_reset(uint16_t mask, uint8_t mode);

int mia_m10_send_assist_data(uint8_t* data, uint32_t size);

#endif /* UBLOX_MIA_M10_H_ */