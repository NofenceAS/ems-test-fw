#ifndef MOCK_UART_H_
#define MOCK_UART_H_

#include <ztest.h>
#include <stdint.h>
#include <inttypes.h>
#include <device.h>

struct uart_config {
	uint32_t baudrate;
	uint8_t parity;
	uint8_t stop_bits;
	uint8_t data_bits;
	uint8_t flow_ctrl;
};
typedef void (*uart_irq_callback_user_data_t)(const struct device *dev,
					      void *user_data);

int uart_configure(const struct device *dev,
			     const struct uart_config *cfg);

int uart_config_get(const struct device *dev,
			      struct uart_config *cfg);

int uart_fifo_fill(const struct device *dev,
				const uint8_t *tx_data,
				int size);

int uart_fifo_read(const struct device *dev, 
				uint8_t *rx_data, 
				const int size);

void uart_irq_tx_enable(const struct device *dev);
void uart_irq_tx_disable(const struct device *dev);
int uart_irq_tx_ready(const struct device *dev);

void uart_irq_rx_enable(const struct device *dev);
void uart_irq_rx_disable(const struct device *dev);
int uart_irq_tx_complete(const struct device *dev);

int uart_irq_rx_ready(const struct device *dev);
void uart_irq_err_enable(const struct device *dev);
void uart_irq_err_disable(const struct device *dev);

int uart_irq_is_pending(const struct device *dev);
int uart_irq_update(const struct device *dev);


void uart_irq_callback_set(const struct device *dev,
				uart_irq_callback_user_data_t cb);


#endif