#define DT_DRV_COMPAT u_blox_mia_m10

#include "ublox-mia-m10.h"

#include <init.h>
#include <zephyr.h>
#include <sys/ring_buffer.h>
#include <drivers/gpio.h>
#include <drivers/uart.h>
#include <logging/log.h>

#include "gnss.h"
#include "ublox_protocol.h"
#include "nmea_parser.h"

LOG_MODULE_REGISTER(MIA_M10, CONFIG_GNSS_LOG_LEVEL);

#define GNSS_UART_NODE			DT_INST_BUS(0)
#define GNSS_UART_DEV			DEVICE_DT_GET(GNSS_UART_NODE)

/* RX thread structures */
K_KERNEL_STACK_DEFINE(gnss_rx_stack,
		      CONFIG_GNSS_MIA_M10_RX_STACK_SIZE);
struct k_thread gnss_rx_thread;

K_KERNEL_STACK_DEFINE(gnss_test_stack,
		      CONFIG_GNSS_MIA_M10_RX_STACK_SIZE);
struct k_thread gnss_test_thread;

static const struct device *mia_m10_uart_dev = GNSS_UART_DEV;

/* Command / Response control structures for parsing */
static struct k_mutex cmd_mutex;
static struct k_sem cmd_ack_sem;
static struct k_sem cmd_poll_sem;
static bool cmd_ack = false;
static uint8_t cmd_buf[256];
static uint32_t cmd_size = 0;

/* TODO - consider creating context struct */
/* Semaphore for signalling thread about received data. */
struct k_sem gnss_rx_sem;

/* GNSS buffers for sending and receiving data. */
static uint8_t* gnss_tx_buffer = NULL;
static struct ring_buf gnss_tx_ring_buf;

static uint8_t* gnss_rx_buffer = NULL;
static uint32_t gnss_rx_cnt = 0;

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
	uint32_t received;
	uint32_t free_space;
	
	free_space = CONFIG_GNSS_MIA_M10_BUFFER_SIZE - gnss_rx_cnt;

	if (free_space == 0) {
		mia_m10_uart_flush(uart_dev);
		return;
	}

	received = uart_fifo_read(uart_dev, 
				  &gnss_rx_buffer[gnss_rx_cnt], 
				  free_space);
	
	gnss_rx_cnt += received;

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

static uint32_t mia_m10_parse_data(uint32_t offset)
{
	uint32_t i = offset;
	uint32_t remainder = gnss_rx_cnt-i;
	while(remainder > 0) {
		if (gnss_rx_buffer[i] == NMEA_START_DELIMITER) {
			/* NMEA */
			uint32_t parsed = 
				nmea_parse(&gnss_rx_buffer[i], remainder);
			if (parsed == 0) {
				/* Nothing parsed means not enough data yet */
				break;
			} else {
				remainder -= parsed;
				i += parsed;
			}
		} else if (gnss_rx_buffer[i] == UBLOX_SYNC_CHAR_1) {
			/* UBLOX */
			uint32_t parsed = 
				ublox_parse(&gnss_rx_buffer[i], remainder);
			if (parsed == 0) {
				/* Nothing parsed means not enough data yet */
				break;
			} else {
				remainder -= parsed;
				i += parsed;
			}
		} else {
			remainder--;
			i++;
		}
	}

	return i - offset;
}

static void mia_m10_rx_consume(uint32_t cnt)
{
	if (cnt < gnss_rx_cnt) {
		for (uint32_t i = 0; i < cnt; i++) {
			gnss_rx_buffer[i] = gnss_rx_buffer[cnt + i];
		}
	}
	gnss_rx_cnt -= cnt;
}

static void mia_m10_handle_received_data(void* dev)
{
	const struct device *uart_dev = (const struct device *)dev;

	bool is_parsing;
	uint32_t total_parsed_cnt;

	while (true) {
		k_sem_take(&gnss_rx_sem, K_FOREVER);

		total_parsed_cnt = 0;
		is_parsing = true;
		while (is_parsing) {
			uint32_t parsed_cnt = 
					mia_m10_parse_data(total_parsed_cnt);

			if (parsed_cnt == 0) {
				is_parsing = false;
			}

			total_parsed_cnt += parsed_cnt;
		}

		/* Consume data from buffer. It is necessary to block
		 * UART RX interrupt to avoid concurrent update of counter
		 * and buffer area. Preemption of thread must be disabled
		 * to minimize the time UART RX interrupts are disabled. 
		*/
		k_sched_lock();
		uart_irq_rx_disable(uart_dev);
		
		mia_m10_rx_consume(total_parsed_cnt);

		uart_irq_rx_enable(uart_dev);
		k_sched_unlock();
	
		k_yield();
	}
}


static void mia_m10_test(void* dev)
{
	while (true) {
		k_sleep(K_MSEC(10000));
		uint64_t raw_value = 0;
		int ret = mia_m10_config_get(UBX_CFG_MSGOUT_NMEA_ID_RMC_UART1,
					     1, &raw_value);
		if (ret != 0)
		{

		}
	}
}

static int mia_m10_init(const struct device *dev)
{
	mia_m10_uart_dev = GNSS_UART_DEV;

	/* Initialize Ublox protocol parser */
	ublox_protocol_init();
	k_mutex_init(&cmd_mutex);
	k_sem_init(&cmd_ack_sem, 0, 1);
	k_sem_init(&cmd_poll_sem, 0, 1);

	/* Check that UART device is available */
	if (mia_m10_uart_dev == NULL) {
		return -EIO;
	}
	if (!device_is_ready(mia_m10_uart_dev)) {
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
		gnss_rx_cnt = 0;
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
	uart_irq_rx_disable(mia_m10_uart_dev);
	uart_irq_tx_disable(mia_m10_uart_dev);

	/* Prepare UART for data reception */
	mia_m10_uart_flush(mia_m10_uart_dev);
	uart_irq_callback_set(mia_m10_uart_dev, mia_m10_uart_isr);
	uart_irq_rx_enable(mia_m10_uart_dev);
	
	/* Start RX thread */
	k_thread_create(&gnss_rx_thread, gnss_rx_stack,
			K_KERNEL_STACK_SIZEOF(gnss_rx_stack),
			(k_thread_entry_t) mia_m10_handle_received_data,
			(void*)mia_m10_uart_dev, NULL, NULL, 
			K_PRIO_COOP(7), 0, K_NO_WAIT);

	k_thread_create(&gnss_test_thread, gnss_test_stack,
			K_KERNEL_STACK_SIZEOF(gnss_test_stack),
			(k_thread_entry_t) mia_m10_test,
			(void*)mia_m10_uart_dev, NULL, NULL, 
			K_PRIO_COOP(7), 0, K_NO_WAIT);

	return 0;
}

int mia_m10_send(uint8_t* buffer, uint32_t size)
{
	ring_buf_put(&gnss_tx_ring_buf, buffer, size);
	uart_irq_tx_enable(mia_m10_uart_dev);
	return 0;
}

static int mia_m10_poll_cb(void* context, uint8_t msg_class, uint8_t msg_id, void* payload, uint32_t length)
{
	if (length <= sizeof(cmd_buf)) {
		cmd_size = length;
		if (payload != NULL) {
			memcpy(cmd_buf, payload, length);
		}
		k_sem_give(&cmd_poll_sem);
	}

	return 0;
}

static int mia_m10_ack_cb(void* context, uint8_t msg_class, uint8_t msg_id, bool ack)
{
	cmd_ack = ack;
	k_sem_give(&cmd_ack_sem);

	return 0;
}

static int mia_m10_send_ubx_cmd(uint8_t* buffer, uint32_t size, bool wait_for_ack, bool wait_for_data)
{
	int ret = 0;

	/* Reset command response parameters */
	k_sem_reset(&cmd_ack_sem);
	k_sem_reset(&cmd_poll_sem);
	ublox_set_response_handlers(buffer,
				    mia_m10_poll_cb, 
				    mia_m10_ack_cb, 
				    NULL);

	/* Send command bytes */
	ret = mia_m10_send(buffer, size);
	if (ret != 0)
	{
		return ret;
	}

	/* Wait for response, poll and ack */
	if (k_sem_take(&cmd_ack_sem, K_MSEC(500)) != 0) {
		LOG_ERR("ACK timed out");
		k_mutex_unlock(&cmd_mutex);
		return -ETIME;
	}
	if (k_sem_take(&cmd_poll_sem, K_MSEC(500)) != 0) {
		LOG_ERR("POLL timed out");
		k_mutex_unlock(&cmd_mutex);
		return -ETIME;
	}

	return 0;
}

int mia_m10_config_get(uint32_t key, uint8_t size, uint64_t* raw_value)
{
	int ret = 0;

	LOG_ERR("CMD start");
	if (k_mutex_lock(&cmd_mutex, K_MSEC(1000)) == 0) {

		ret = ublox_build_cfg_valget(cmd_buf, &cmd_size, 256,
					     DEFAULT_LAYER, 0, 
					     key);
		if (ret != 0) {
			k_mutex_unlock(&cmd_mutex);
			return ret;
		}

		ret = mia_m10_send_ubx_cmd(cmd_buf, cmd_size, true, true);
		if (ret != 0) {
			k_mutex_unlock(&cmd_mutex);
			return ret;
		}

		/* Parse result payload */
		ret = ublox_get_cfg_val(cmd_buf, cmd_size, size, raw_value);
		if (ret != 0) {
			k_mutex_unlock(&cmd_mutex);
			return ret;
		}

		LOG_ERR("CMD OK");

		k_mutex_unlock(&cmd_mutex);
	} else {
		return -EBUSY;
	}

	return 0;
}

static struct mia_m10_dev_data mia_m10_data;

static const struct mia_m10_dev_config mia_m10_config = {
	.uart_name = DT_INST_BUS_LABEL(0),
};

DEVICE_DT_INST_DEFINE(0, mia_m10_init, NULL,
		    &mia_m10_data, &mia_m10_config, POST_KERNEL,
		    CONFIG_GNSS_INIT_PRIORITY, &mia_m10_api_funcs);
