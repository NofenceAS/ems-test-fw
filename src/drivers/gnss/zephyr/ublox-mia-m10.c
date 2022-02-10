
#define DT_DRV_COMPAT u_blox_mia_m10

#include <init.h>
#include <drivers/gpio.h>
#include <drivers/uart.h>
#include <logging/log.h>

#include "gnss.h"
#include "ublox-mia-m10.h"

LOG_MODULE_REGISTER(MIA_M10, CONFIG_GNSS_LOG_LEVEL);

#define GNSS_UART_NODE			DT_INST_BUS(0)
#define GNSS_UART_DEV			DEVICE_DT_GET(GNSS_UART_NODE)


/* GNSS buffers for sending and receiving data. 
 * Mutex is used to protect buffers. 
*/
static uint8_t* gnss_tx_buffer = NULL;
static struct ring_buf gnss_tx_ring_buf;

/* GNSS buffers for receiving data. */
static uint8_t* gnss_rx_buffer = NULL;
static struct ring_buf gnss_rx_ring_buf;

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

/**
 * @brief Flushes all data from UART device. 
 * 
 * @param[in] uart_dev UART device to flush. 
 */
static void mia_m10_uart_flush(const struct device *uart_dev)
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
static void mia_m10_uart_handle_tx(const struct device *uart_dev)
{
	int bytes = 
		uart_fifo_fill(uart_dev, 
			       &gnss_tx_buffer[gnss_tx_sent], 
			       gnss_tx_count - gnss_tx_sent);
	gnss_tx_sent += bytes;
	
	if (gnss_tx_sent >= gnss_tx_count) {
		atomic_clear_bit(&gnss_tx_in_progress, 0);
		gnss_tx_count = 0;
		gnss_tx_sent = 0;
		uart_irq_tx_disable(uart_dev);
	}
}

/**
 * @brief Handles receive operation from UART. 
 * 
 * @param[in] uart_dev UART device to receive from. 
 */
static void mia_m10_uart_handle_rx(const struct device *uart_dev)
{	
	uint8_t* data;
	uint32_t partial_size = 
		ring_buf_put_claim(&gnss_rx_ring_buf, 
				   &data, 
				   CONFIG_GNSS_MIA_M10_BUFFER_SIZE);

	if (partial_size == 0) {
		passthrough_uart_flush(uart_dev);
		return;
	}

	uint32_t received = uart_fifo_read(uart_dev, 
					   data, 
					   partial_size);
	
	ring_buf_put_finish(&gnss_ring_buf, received);
}

/**
 * @brief Handles interrupts from UART device.
 * 
 * @param[in] uart_dev UART device to handle interrupts for. 
 */
static void mia_m10_uart_isr(const struct device *uart_dev,
				 void *user_data)
{
	while (uart_irq_update(uart_dev) &&
	       uart_irq_is_pending(uart_dev)) {
		
		if (uart_irq_tx_ready(uart_dev)) {
			mia_m10_uart_handle_tx(uart_dev);
		}

		if (uart_irq_rx_ready(uart_dev)) {
			mia_m10_uart_handle_rx(uart_dev);
		}
	}
}

static int mia_m10_init(const struct device *dev)
{
	const struct device *uart_dev = GNSS_UART_DEV;

	/* Check that UART device is available */
	if (uart_dev == NULL) {
		return -EIO;
	}
	if (!device_is_ready(uart_dev)) {
		return -EIO;
	}
	
	/* Allocate buffer if not already done */
	if (gnss_rx_buffer == NULL)
	{
		gnss_rx_buffer = 
			k_malloc(CONFIG_GNSS_MIA_M10_BUFFER_SIZE);

		if (gnss_rx_buffer == NULL) {
			return -ENOBUFS;
		}
		ring_buf_init(&gnss_rx_ring_buf, 
			      CONFIG_GNSS_MIA_M10_BUFFER_SIZE, 
			      gnss_rx_buffer);
	}

	/* Make sure interrupts are disabled */
	uart_irq_rx_disable(uart_dev);
	uart_irq_tx_disable(uart_dev);

	/* Prepare UART for data reception */
	mia_m10_uart_flush(uart_dev);
	uart_irq_callback_set(uart_dev, passthrough_uart_isr);
	uart_irq_rx_enable(uart_dev);

	return 0;
}

static struct mia_m10_dev_data mia_m10_data;

static const struct mia_m10_dev_config mia_m10_config = {
	.uart_name = DT_INST_BUS_LABEL(0),
};

DEVICE_DT_INST_DEFINE(0, mia_m10_init, NULL,
		    &mia_m10_data, &mia_m10_config, POST_KERNEL,
		    CONFIG_GNSS_INIT_PRIORITY, &mia_m10_api_funcs);
