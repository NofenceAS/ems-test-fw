#include "mock_uart.h"
#include <zephyr.h>

struct uart_config current_config = {
	.baudrate = 38400,
	.parity = 0,
	.stop_bits = 0,
	.data_bits = 0,
	.flow_ctrl = 0
};

int uart_configure(const struct device *dev,
			     const struct uart_config *cfg)
{
	printk("uart_configure");
	memcpy(&current_config, cfg, sizeof(struct uart_config));

	return ztest_get_return_value();
}

int uart_config_get(const struct device *dev,
			      struct uart_config *cfg)
{
	printk("uart_config_get");
	memcpy(cfg, &current_config, sizeof(struct uart_config));

	return ztest_get_return_value();
}

int uart_fifo_fill(const struct device *dev,
				const uint8_t *tx_data,
				int size)
{
	printk("uart_fifo_fill");
	return ztest_get_return_value();
}

int uart_fifo_read(const struct device *dev, uint8_t *rx_data, const int size)
{
	printk("uart_fifo_read");
	//ztest_check_expected_value(dev);
	//ztest_copy_return_data(rx_data, size);

	return ztest_get_return_value();
}

void uart_irq_tx_enable(const struct device *dev)
{
	printk("uart_irq_tx_enable");
}

void uart_irq_tx_disable(const struct device *dev)
{
	printk("uart_irq_tx_disable");
}

int uart_irq_tx_ready(const struct device *dev)
{
	printk("uart_irq_tx_ready");
	return ztest_get_return_value();
}

void uart_irq_rx_enable(const struct device *dev)
{
	printk("uart_irq_rx_enable");
}

void uart_irq_rx_disable(const struct device *dev)
{
	printk("uart_irq_rx_disable");
}

int uart_irq_tx_complete(const struct device *dev)
{
	printk("uart_irq_tx_complete");
	return ztest_get_return_value();
}

int uart_irq_rx_ready(const struct device *dev)
{
	printk("uart_irq_rx_ready");
	return ztest_get_return_value();
}

void uart_irq_err_enable(const struct device *dev)
{
	printk("uart_irq_err_enable");
}

void uart_irq_err_disable(const struct device *dev)
{
	printk("uart_irq_err_disable");
}

int uart_irq_is_pending(const struct device *dev)
{
	printk("uart_irq_is_pending");
	return ztest_get_return_value();
}

int uart_irq_update(const struct device *dev)
{
	printk("uart_irq_update");
	return ztest_get_return_value();
}

void uart_irq_callback_set(const struct device *dev,
				uart_irq_callback_user_data_t cb)
{
	printk("uart_irq_callback_set");
}
