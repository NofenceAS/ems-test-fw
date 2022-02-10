
#define DT_DRV_COMPAT u_blox_mia_m10

#include <init.h>
#include <zephyr.h>
#include <sys/ring_buffer.h>
#include <drivers/gpio.h>
#include <drivers/uart.h>
#include <logging/log.h>

#include "gnss.h"
#include "ublox-mia-m10.h"

LOG_MODULE_REGISTER(MIA_M10, CONFIG_GNSS_LOG_LEVEL);

#define GNSS_UART_NODE			DT_INST_BUS(0)
#define GNSS_UART_DEV			DEVICE_DT_GET(GNSS_UART_NODE)

/* RX thread structures */
K_KERNEL_STACK_DEFINE(gnss_rx_stack,
		      CONFIG_GNSS_MIA_M10_RX_STACK_SIZE);
struct k_thread gnss_rx_thread;

/* TODO - consider creating context struct */
/* Semaphore for signalling thread about received data. */
struct k_sem gnss_rx_sem;

/* GNSS buffers for sending and receiving data. */
static uint8_t* gnss_tx_buffer = NULL;
static struct ring_buf gnss_tx_ring_buf;

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
	int err;
	uint32_t size;
	uint32_t sent;
	uint8_t* data;

	size = ring_buf_get_claim(&gnss_tx_ring_buf, 
					&data, 
					CONFIG_GNSS_MIA_M10_BUFFER_SIZE);
	
	sent = uart_fifo_fill(uart_dev, data, size);
	
	err = ring_buf_get_finish(&gnss_tx_ring_buf, sent);
	if (err != 0) {
		LOG_ERR("Failed finishing GNSS TX buffer operation.");
	}
	
	if (ring_buf_is_empty(&gnss_tx_ring_buf)) {
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
	int ret;
	uint8_t* data;

	uint32_t partial_size = 
		ring_buf_put_claim(&gnss_rx_ring_buf, 
				&data, 
				CONFIG_GNSS_MIA_M10_BUFFER_SIZE);

	if (partial_size == 0) {
		mia_m10_uart_flush(uart_dev);
		return;
	}

	uint32_t received = uart_fifo_read(uart_dev, 
					data, 
					partial_size);
	
	ret = ring_buf_put_finish(&gnss_rx_ring_buf, received);
	if (ret != 0) {
		LOG_ERR("Failed finishing GNSS RX buffer operation.");
	}

	if (received > 0) {
		k_sem_give(&gnss_rx_sem);
	}
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

static void mia_m10_parse_rx(void)
{
	int err;
	uint32_t size;
	uint32_t sent;
	uint8_t* data;

	while (true) {
		k_sem_take(&gnss_rx_sem, K_FOREVER);

		size = ring_buf_get_claim(&gnss_rx_ring_buf, 
						&data, 
						CONFIG_GNSS_MIA_M10_BUFFER_SIZE);
		
		/* TODO - Parse received bytes */
		sent = size; 
		
		err = ring_buf_get_finish(&gnss_rx_ring_buf, sent);
		if (err != 0) {
			LOG_ERR("Failed finishing GNSS RX buffer operation.");
		}
	
		k_yield();
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
	if (gnss_tx_buffer == NULL)
	{
		gnss_tx_buffer = 
			k_malloc(CONFIG_GNSS_MIA_M10_BUFFER_SIZE);

		if (gnss_tx_buffer == NULL) {
			return -ENOBUFS;
		}
		ring_buf_init(&gnss_tx_ring_buf, 
			      CONFIG_GNSS_MIA_M10_BUFFER_SIZE, 
			      gnss_tx_buffer);
	}
	
	k_sem_init(&gnss_rx_sem, 0, 1);

	/* Make sure interrupts are disabled */
	uart_irq_rx_disable(uart_dev);
	uart_irq_tx_disable(uart_dev);

	/* Prepare UART for data reception */
	mia_m10_uart_flush(uart_dev);
	uart_irq_callback_set(uart_dev, mia_m10_uart_isr);
	uart_irq_rx_enable(uart_dev);
	
	/* Start RX thread */
	k_thread_create(&gnss_rx_thread, gnss_rx_stack,
			K_KERNEL_STACK_SIZEOF(gnss_rx_stack),
			(k_thread_entry_t) mia_m10_parse_rx,
			NULL, NULL, NULL, K_PRIO_COOP(7), 0, K_NO_WAIT);

	return 0;
}

static struct mia_m10_dev_data mia_m10_data;

static const struct mia_m10_dev_config mia_m10_config = {
	.uart_name = DT_INST_BUS_LABEL(0),
};

DEVICE_DT_INST_DEFINE(0, mia_m10_init, NULL,
		    &mia_m10_data, &mia_m10_config, POST_KERNEL,
		    CONFIG_GNSS_INIT_PRIORITY, &mia_m10_api_funcs);
