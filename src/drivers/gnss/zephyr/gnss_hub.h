#ifndef GNSS_HUB_H_
#define GNSS_HUB_H_

#include <zephyr.h>
#include <device.h>
#include <drivers/uart.h>

#include <stdint.h>
#include <stdbool.h>

#define GNSS_HUB_ID_UART			0
#define GNSS_HUB_ID_DRIVER			1
#define GNSS_HUB_ID_DIAGNOSTICS		2

#define GNSS_HUB_MODE_DEFAULT		0
#define GNSS_HUB_MODE_SNIFFER		1
#define GNSS_HUB_MODE_CONTROLLER	2
#define GNSS_HUB_MODE_EMULATOR		3

int gnss_hub_init(const struct device *uart_dev, 
		  struct k_sem* rx_sem, 
		  uint32_t baudrate);

int gnss_hub_configure(uint8_t mode);

int gnss_hub_set_uart_baudrate(uint32_t baudrate, bool immediate);

uint32_t gnss_hub_get_uart_baudrate(void);

int gnss_hub_send(uint8_t hub_id, uint8_t* buffer, uint32_t cnt);

bool gnss_hub_rx_is_empty(uint8_t hub_id);

int gnss_hub_rx_get_data(uint8_t hub_id, uint8_t** buffer, uint32_t* cnt);

int gnss_hub_rx_consume(uint8_t hub_id, uint32_t cnt);

#endif /* GNSS_UART_H_ */