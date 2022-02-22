#ifndef UART_MOCK_H_
#define UART_MOCK_H_

#include <zephyr.h>
#include <device.h>

void mock_uart_send(const struct device *dev, uint8_t* data, uint32_t size);

#endif /* UART_MOCK_H_ */