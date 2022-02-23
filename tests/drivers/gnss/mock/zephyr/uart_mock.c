#include "uart_mock.h"

#include <sys/ring_buffer.h>
#include <drivers/uart.h>

#define DT_DRV_COMPAT	nofence_mock_uart

#define BAUDRATE	DT_INST_PROP(0, current_speed)

struct mock_uart_data {
	struct uart_config uart_config;
};

static uart_irq_callback_user_data_t irq_callback;
static void *irq_cb_data;

//#define MOCK_UART_VERBOSE

/* GNSS to MCU */ 
#define UART_MOCK_RX_SIZE 1024
static uint8_t uart_mock_rx_buffer[UART_MOCK_RX_SIZE];
static struct ring_buf uart_mock_rx_ring_buf;

static struct k_sem* uart_mock_rx_sem;

/* MCU to GNSS */
#define UART_MOCK_TX_SIZE 1024
static uint8_t uart_mock_tx_buffer[UART_MOCK_TX_SIZE];
static struct ring_buf uart_mock_tx_ring_buf;

static int mock_uart_poll_in(const struct device *dev, unsigned char *c)
{
	return 0;
}

static void mock_uart_poll_out(const struct device *dev, unsigned char c)
{

}

static int mock_uart_err_check(const struct device *dev)
{
	return 0;
}

static int mock_uart_configure(const struct device *dev,
			       const struct uart_config *cfg)
{
#ifdef MOCK_UART_VERBOSE
	printk("%d: mock_uart_configure\r\n", k_uptime_get_32());
#endif
	struct mock_uart_data* data = dev->data;
	data->uart_config = *cfg;
	return 0;
}

static int mock_uart_config_get(const struct device *dev,
				struct uart_config *cfg)
{
#ifdef MOCK_UART_VERBOSE
	printk("%d: mock_uart_config_get\r\n", k_uptime_get_32());
#endif
	struct mock_uart_data* data = dev->data;
	*cfg = data->uart_config;
	return 0;
}


static int mock_uart_fifo_fill(const struct device *dev,
			       const uint8_t *tx_data,
			       int len)
{
	(void)dev;
	
#ifdef MOCK_UART_VERBOSE
	printk("%d: ", k_uptime_get_32());
	for (int i = 0; i < len; i++)
	{
		printk("0x%02X ", tx_data[i]);
	}
	printk("\r\n");
#endif

	int cnt = 0;
	if (len >= 1) {
		ring_buf_put(&uart_mock_tx_ring_buf, tx_data, 1);
		k_sem_give(uart_mock_rx_sem);
		cnt = 1;
	}

	return cnt;
}

static int mock_uart_fifo_read(const struct device *dev,
			       uint8_t *rx_data,
			       const int size)
{
	if (ring_buf_is_empty(&uart_mock_rx_ring_buf)) {
		/* No new data */
		return 0;
	}

	uint8_t* data;
	uint32_t cnt = ring_buf_get_claim(
				&uart_mock_rx_ring_buf, 
				&data, 
				UART_MOCK_RX_SIZE);
	
	if (cnt > size) {
		cnt = size;
	}
	memcpy(rx_data, data, cnt);

	ring_buf_get_finish(&uart_mock_rx_ring_buf, cnt);

	return cnt;
}

static bool tx_irq_enabled = false;

static void mock_uart_irq_tx_enable(const struct device *dev)
{
	tx_irq_enabled = true;
	
#ifdef MOCK_UART_VERBOSE
	printk("%d: mock_uart_irq_tx_enable\r\n", k_uptime_get_32());
#endif
	if (irq_callback) {
		irq_callback(dev, irq_cb_data);
	}
}

static void mock_uart_irq_tx_disable(const struct device *dev)
{
#ifdef MOCK_UART_VERBOSE
	printk("%d: mock_uart_irq_tx_disable\r\n", k_uptime_get_32());
#endif
	tx_irq_enabled = false;
}

static void mock_uart_irq_rx_enable(const struct device *dev)
{
	
}

static void mock_uart_irq_rx_disable(const struct device *dev)
{
	
}

static int mock_uart_irq_tx_ready(const struct device *dev)
{
#ifdef MOCK_UART_VERBOSE
	printk("%d: mock_uart_irq_tx_ready\r\n", k_uptime_get_32());
#endif
	if (tx_irq_enabled) {
		if (ring_buf_is_empty(&uart_mock_tx_ring_buf)) {
#ifdef MOCK_UART_VERBOSE
			printk("True\r\n");
#endif
			return 1;
		}
	}
	return 0;
}

static int mock_uart_irq_tx_complete(const struct device *dev)
{
#ifdef MOCK_UART_VERBOSE
	printk("%d: mock_uart_irq_tx_complete\r\n", k_uptime_get_32());
#endif
	if (tx_irq_enabled) {
		if (ring_buf_is_empty(&uart_mock_tx_ring_buf)) {
#ifdef MOCK_UART_VERBOSE
			printk("True\r\n");
#endif
			return 1;
		}
	}
	return 0;
}

static int mock_uart_irq_rx_ready(const struct device *dev)
{
#ifdef MOCK_UART_VERBOSE
	printk("%d: mock_uart_irq_rx_ready\r\n", k_uptime_get_32());
#endif
	if (!ring_buf_is_empty(&uart_mock_rx_ring_buf)) {
#ifdef MOCK_UART_VERBOSE
		printk("True\r\n");
#endif
		return 1;
	}
	return 0;
}

static void mock_uart_irq_err_enable(const struct device *dev)
{
	
}

static void mock_uart_irq_err_disable(const struct device *dev)
{
	
}

static int mock_uart_irq_is_pending(const struct device *dev)
{
	return mock_uart_irq_rx_ready(dev) || \
		mock_uart_irq_tx_ready(dev) || \
		mock_uart_irq_tx_complete(dev);
}

static int mock_uart_irq_update(const struct device *dev)
{
	return 1;
}

static void mock_uart_irq_callback_set(const struct device *dev,
				       uart_irq_callback_user_data_t cb,
				       void *cb_data)
{
	(void)dev;
	irq_callback = cb;
	irq_cb_data = cb_data;
}

int mock_uart_register_sem(const struct device *dev, struct k_sem* rx_sem)
{
	uart_mock_rx_sem = rx_sem;

	return 0;
}

int mock_uart_send(const struct device *dev, uint8_t* data, uint32_t size)
{
	if (size == 0) {
		return 0;
	}

	ring_buf_put(&uart_mock_rx_ring_buf, data, size);

#ifdef MOCK_UART_VERBOSE
	printk("Sending: ");
	for (int i = 0; i < cnt; i++)
	{
		printk("0x%02X ", rx_data[i]);
	}
	printk("\r\n");
#endif

	if (irq_callback) {
		irq_callback(dev, irq_cb_data);
	}

	return 0;
}

int mock_uart_receive(const struct device *dev, 
		      uint8_t* data, 
		      uint32_t* size, 
		      uint32_t max_size,
		      bool consume)
{
#ifdef MOCK_UART_VERBOSE
	printk("%d: mock_uart_receive\r\n", k_uptime_get_32());
#endif
	
	uint32_t cnt = ring_buf_get(&uart_mock_tx_ring_buf, data, max_size);
	
	if (consume && (cnt > 0)) {
		if (irq_callback) {
#ifdef MOCK_UART_VERBOSE
			printk("%d: irq_callback\r\n", k_uptime_get_32());
#endif
			irq_callback(dev, irq_cb_data);
		}
	}

	*size = cnt;

	return 0;
}

static int mock_uart_init(const struct device *dev)
{	
	ring_buf_init(&uart_mock_tx_ring_buf, 
			UART_MOCK_TX_SIZE, 
			uart_mock_tx_buffer);

	ring_buf_init(&uart_mock_rx_ring_buf, 
			UART_MOCK_RX_SIZE, 
			uart_mock_rx_buffer);

	/* Garbage bytes on startup */
	uint8_t garbage[] = {0x13, 0x37};
	mock_uart_send(dev, garbage, sizeof(garbage));

	return 0;
}

static const struct uart_driver_api mock_uart_driver_api = {
	.poll_in          = mock_uart_poll_in,
	.poll_out         = mock_uart_poll_out,
	.err_check        = mock_uart_err_check,

	.configure        = mock_uart_configure,
	.config_get       = mock_uart_config_get,

	.fifo_fill        = mock_uart_fifo_fill,
	.fifo_read        = mock_uart_fifo_read,
	.irq_tx_enable    = mock_uart_irq_tx_enable,
	.irq_tx_disable   = mock_uart_irq_tx_disable,
	.irq_tx_ready     = mock_uart_irq_tx_ready,
	.irq_rx_enable    = mock_uart_irq_rx_enable,
	.irq_rx_disable   = mock_uart_irq_rx_disable,
	.irq_tx_complete  = mock_uart_irq_tx_complete,
	.irq_rx_ready     = mock_uart_irq_rx_ready,
	.irq_err_enable   = mock_uart_irq_err_enable,
	.irq_err_disable  = mock_uart_irq_err_disable,
	.irq_is_pending   = mock_uart_irq_is_pending,
	.irq_update       = mock_uart_irq_update,
	.irq_callback_set = mock_uart_irq_callback_set,
};

static struct mock_uart_data data_mock = {
	.uart_config = {
		.stop_bits = UART_CFG_STOP_BITS_1,
		.data_bits = UART_CFG_DATA_BITS_8,
		.baudrate  = BAUDRATE,
		.parity    = UART_CFG_PARITY_NONE,
		.flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
	}
};

DEVICE_DT_INST_DEFINE(0,
	      mock_uart_init,
	      NULL,
	      &data_mock,
	      NULL,
	      /* Initialize UART device before UART console. */
	      PRE_KERNEL_1,
	      10,
	      &mock_uart_driver_api);
