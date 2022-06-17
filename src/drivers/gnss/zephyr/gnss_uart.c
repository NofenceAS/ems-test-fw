#include "gnss_uart.h"

#include "gnss_hub.h"

#include <zephyr.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(GNSS_UART, CONFIG_GNSS_LOG_LEVEL);

/* GNSS UART device */
static const struct device *gnss_uart_dev = NULL;

/* Baudrate changing can be done by setting baudrate value before signalling 
 * the change in the atomic variable. This is then checked and processed 
 * during TX completed interrupt to assure it is done at the exact right 
 * time. Some GNSS will send the ack to the baudrate change using new baudrate.
 */
static atomic_t gnss_uart_baudrate_change_req = ATOMIC_INIT(0x0);
static uint32_t gnss_uart_baudrate = 0;

/**
 * @brief Flushes all data from UART device. 
 * 
 * @param[in] uart_dev UART device to flush. 
 */
static void gnss_uart_flush(const struct device *uart_dev)
{
	uint8_t c;

	while (uart_fifo_read(uart_dev, &c, 1) > 0) {
		continue;
	}
}

/**
 * @brief Handles transmit operation to UART. 
 * 
 * @param[in] uart_dev UART device to send to. 
 */
static void gnss_uart_handle_tx(const struct device *uart_dev)
{
	int err;
	uint32_t size;
	uint32_t sent;
	uint8_t* data;

	if (gnss_hub_rx_get_data(GNSS_HUB_ID_UART, &data, &size) == 0) {
		sent = uart_fifo_fill(uart_dev, data, size);

		err = gnss_hub_rx_consume(GNSS_HUB_ID_UART, sent);
		if (err != 0) {
			LOG_ERR("Failed finishing GNSS TX buffer operation.");
		}
	}
}

/**
 * @brief Handles completion of UART transaction. 
 * 
 * @param[in] uart_dev UART device to send to. 
 */
static void gnss_uart_handle_complete(const struct device *uart_dev)
{	
	if (gnss_hub_rx_is_empty(GNSS_HUB_ID_UART)) {
		uart_irq_tx_disable(uart_dev);

		if (atomic_test_and_clear_bit(
				&gnss_uart_baudrate_change_req, 0)) {
			
			gnss_uart_set_baudrate(gnss_uart_baudrate, true);
		}
	}
}

/**
 * @brief Handles receive operation from UART. 
 * 
 * @param[in] uart_dev UART device to receive from. 
 */
static void gnss_uart_handle_rx(const struct device *uart_dev)
{
	uint32_t received = 0;
	uint8_t buf[8];

	do {
		received = uart_fifo_read(uart_dev, buf, 8);
		gnss_hub_send(GNSS_HUB_ID_UART, buf, received);
	} while (received > 0);
}

/**
 * @brief Handles interrupts from UART device.
 * 
 * @param[in] uart_dev UART device to handle interrupts for. 
 */
static void gnss_uart_isr(const struct device *uart_dev,
				 void *user_data)
{
	while (uart_irq_update(uart_dev) &&
	       uart_irq_is_pending(uart_dev)) {
		
		if (uart_irq_tx_ready(uart_dev)) {
			gnss_uart_handle_tx(uart_dev);
		}

		if (uart_irq_tx_complete(uart_dev)) {
			gnss_uart_handle_complete(uart_dev);
		}

		if (uart_irq_rx_ready(uart_dev)) {
			gnss_uart_handle_rx(uart_dev);
		}
	}
}

int gnss_uart_init(const struct device *uart_dev, 
		   struct k_sem* rx_sem, 
		   uint32_t baudrate)
{
	if (uart_dev == NULL) {
		return -EINVAL;
	}
	if (rx_sem == NULL) {
		return -EINVAL;
	}

	gnss_uart_dev = uart_dev;
	gnss_uart_baudrate = baudrate;

	/* Make sure interrupts are disabled */
	uart_irq_rx_disable(gnss_uart_dev);
	uart_irq_tx_disable(gnss_uart_dev);

	/* Prepare UART for data reception */
	gnss_uart_flush(gnss_uart_dev);
	uart_irq_callback_set(gnss_uart_dev, gnss_uart_isr);
	uart_irq_rx_enable(gnss_uart_dev);

	return 0;
}

uint32_t gnss_uart_get_baudrate(void)
{
	struct uart_config cfg;

	int ret = uart_config_get(gnss_uart_dev, &cfg);
	if (ret != 0) {
		return 0;
	}
	
	return cfg.baudrate;
}

int gnss_uart_set_baudrate(uint32_t baudrate, bool immediate)
{
	int ret = 0;
	if (immediate) {
		struct uart_config cfg;

		if (baudrate == 0) {
			return -EINVAL;
		}
		ret = uart_config_get(gnss_uart_dev, &cfg);
		if (ret != 0) {
			return -EIO;
		}
		if (cfg.baudrate == baudrate) {
			return 0;
		}

		cfg.baudrate = baudrate;
		ret = uart_configure(gnss_uart_dev, &cfg);
		if (ret != 0) {
			return -EIO;
		}
	} else {
		/* Requested baudrate change will be performed by ISR on 
		 * completed transaction of command */
		gnss_uart_baudrate = baudrate;
		atomic_set_bit(&gnss_uart_baudrate_change_req, 0);
	}

	return ret;
}

int gnss_uart_start_send(void)
{
	uart_irq_tx_enable(gnss_uart_dev);
	return 0;
}

void gnss_uart_block(bool block)
{
	if (block) {
		k_sched_lock();
		uart_irq_rx_disable(gnss_uart_dev);
	} else {
		uart_irq_rx_enable(gnss_uart_dev);
		k_sched_unlock();
	}
}