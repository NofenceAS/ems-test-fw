#include "gnss_hub.h"

#include "gnss_uart.h"

static uint8_t hub_mode = GNSS_HUB_MODE_DEFAULT;

int gnss_hub_init(const struct device *uart_dev, 
		  struct k_sem* rx_sem, 
		  uint32_t baudrate)
{
	int err = 0;
	
	err = gnss_uart_init(uart_dev, rx_sem, baudrate);

	return err;
}

int gnss_hub_configure(uint8_t mode)
{
	if (mode > GNSS_HUB_MODE_EMULATOR) {
		return -EINVAL;
	}

	/** @todo Implement more modes */
	if (mode != GNSS_HUB_MODE_DEFAULT) {
		return -EINVAL;
	}

	hub_mode = mode;

	return 0;
}

int gnss_hub_set_uart_baudrate(uint32_t baudrate, bool immediate)
{
	return gnss_uart_set_baudrate(baudrate, immediate);
}

uint32_t gnss_hub_get_uart_baudrate(void)
{
	return gnss_uart_get_baudrate();
}

int gnss_hub_send(uint8_t hub_id, uint8_t* buffer, uint32_t cnt)
{
	int err = 0;

	if (hub_id == GNSS_HUB_ID_DRIVER) {
		err = gnss_uart_send(buffer, cnt);
	} else {
		err = -EIO;
	}

	return err;
}

int gnss_hub_rx_get_data(uint8_t hub_id, uint8_t** buffer, uint32_t* cnt)
{
	int err = 0;

	if (hub_id == GNSS_HUB_ID_DRIVER) {
		gnss_uart_rx_get_data(buffer, cnt);
	} else {
		err = -EIO;
	}

	return err;
}

int gnss_hub_rx_consume(uint8_t hub_id, uint32_t cnt)
{
	int err = 0;

	if (hub_id == GNSS_HUB_ID_DRIVER) {
		gnss_uart_rx_consume(cnt);
	} else {
		err = -EIO;
	}

	return err;
}