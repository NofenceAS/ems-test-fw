#ifndef UART_MOCK_H_
#define UART_MOCK_H_

#include <zephyr.h>
#include <device.h>

int mock_uart_register_sem(const struct device *dev, struct k_sem* rx_sem);
int mock_uart_send(const struct device *dev, uint8_t* data, uint32_t size);
int mock_uart_receive(const struct device *dev, 
		      uint8_t* data, 
		      uint32_t* size, 
		      uint32_t max_size,
		      bool consume);
int mock_uart_consume(const struct device *dev, uint32_t cnt);

#endif /* UART_MOCK_H_ */