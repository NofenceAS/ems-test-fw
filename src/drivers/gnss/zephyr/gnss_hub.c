#include "gnss_hub.h"

#include "gnss_uart.h"

#include <sys/ring_buffer.h>

/* GNSS buffers for sending and receiving data. */
static uint8_t* gnss_tx_buffer = NULL;
static struct ring_buf gnss_tx_ring_buf;

static uint8_t* gnss_rx_buffer = NULL;
static uint32_t gnss_rx_cnt = 0;

/* Semaphore for notifying about available data */
static struct k_sem* gnss_rx_sem;

static uint8_t hub_mode = GNSS_HUB_MODE_DEFAULT;

int gnss_hub_init(const struct device *uart_dev, 
		  struct k_sem* rx_sem, 
		  uint32_t baudrate)
{
	int err = 0;
	
	gnss_rx_sem = rx_sem;
	
	/* Allocate buffer if not already done */
	if (gnss_rx_buffer == NULL)
	{
		gnss_rx_buffer = 
			k_malloc(CONFIG_GNSS_COMM_BUFFER_SIZE);

		if (gnss_rx_buffer == NULL) {
			return -ENOBUFS;
		}
		gnss_rx_cnt = 0;
	}
	if (gnss_tx_buffer == NULL)
	{
		gnss_tx_buffer = 
			k_malloc(CONFIG_GNSS_COMM_BUFFER_SIZE);

		if (gnss_tx_buffer == NULL) {
			return -ENOBUFS;
		}
		ring_buf_init(&gnss_tx_ring_buf, 
			      CONFIG_GNSS_COMM_BUFFER_SIZE, 
			      gnss_tx_buffer);
	}

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
		ring_buf_put(&gnss_tx_ring_buf, buffer, cnt);
		gnss_uart_start_send();
	} else if (hub_id == GNSS_HUB_ID_UART) {
		uint32_t free_space = CONFIG_GNSS_COMM_BUFFER_SIZE - 
				      gnss_rx_cnt;
		uint32_t to_copy = MIN(cnt, free_space);
		memcpy(&gnss_rx_buffer[gnss_rx_cnt], buffer, to_copy);
		gnss_rx_cnt += to_copy;
		k_sem_give(gnss_rx_sem);
	} else {
		err = -EIO;
	}

	return err;
}

bool gnss_hub_rx_is_empty(uint8_t hub_id)
{
	if (hub_id == GNSS_HUB_ID_DRIVER) {
		return (gnss_rx_cnt == 0);
	} else if (hub_id == GNSS_HUB_ID_UART) {
		return ring_buf_is_empty(&gnss_tx_ring_buf);
	}

	return false;
}

int gnss_hub_rx_get_data(uint8_t hub_id, uint8_t** buffer, uint32_t* cnt)
{
	int err = 0;

	if (hub_id == GNSS_HUB_ID_DRIVER) {
		*cnt = gnss_rx_cnt;
		*buffer = gnss_rx_buffer;
	} else if (hub_id == GNSS_HUB_ID_UART) {
		*cnt = ring_buf_get_claim(&gnss_tx_ring_buf, 
					  buffer, 
					  CONFIG_GNSS_COMM_BUFFER_SIZE);
	} else {
		err = -EIO;
	}

	return err;
}

int gnss_hub_rx_consume(uint8_t hub_id, uint32_t cnt)
{
	int err = 0;

	if (hub_id == GNSS_HUB_ID_DRIVER) {
		if (cnt > gnss_rx_cnt) {
			cnt = gnss_rx_cnt;
		}

		/* Consume data from buffer. It is necessary to block
		* UART RX interrupt to avoid concurrent update of counter
		* and buffer area. Preemption of thread must be disabled
		* to minimize the time UART RX interrupts are disabled. 
		*/
		gnss_uart_block(true);

		memmove(gnss_rx_buffer, &gnss_rx_buffer[cnt], cnt);
		gnss_rx_cnt -= cnt;

		gnss_uart_block(false);
		
	} else if (hub_id == GNSS_HUB_ID_UART) {
		err = ring_buf_get_finish(&gnss_tx_ring_buf, cnt);
	} else {
		err = -EIO;
	}

	return err;
}