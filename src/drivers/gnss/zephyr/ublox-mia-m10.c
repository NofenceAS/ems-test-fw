#define DT_DRV_COMPAT u_blox_mia_m10

#include "ublox-mia-m10.h"

#include <init.h>
#include <zephyr.h>
#include <sys/ring_buffer.h>
#include <drivers/gpio.h>
#include <drivers/uart.h>
#include <logging/log.h>
#include <sys/timeutil.h>

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

static const struct device *mia_m10_uart_dev = GNSS_UART_DEV;

/* Command / Response control structures for parsing */
static struct k_mutex cmd_mutex;
static struct k_sem cmd_ack_sem;
static struct k_sem cmd_poll_sem;
static bool cmd_ack = false;
static uint8_t cmd_buf[CONFIG_GNSS_MIA_M10_CMD_MAX_SIZE];
static uint32_t cmd_size = 0;

/* TODO - consider creating context struct */
/* Semaphore for signalling thread about received data. */
struct k_sem gnss_rx_sem;

/* GNSS buffers for sending and receiving data. */
static uint8_t* gnss_tx_buffer = NULL;
static struct ring_buf gnss_tx_ring_buf;

static uint8_t* gnss_rx_buffer = NULL;
static uint32_t gnss_rx_cnt = 0;

static atomic_t mia_m10_baudrate_change_req = ATOMIC_INIT(0x0);
static uint32_t mia_m10_baudrate = MIA_M10_DEFAULT_BAUDRATE;

#define GNSS_DATA_FLAG_NAV_DOP		(1<<0)
#define GNSS_DATA_FLAG_NAV_PVT		(1<<1)
#define GNSS_DATA_FLAG_NAV_STATUS	(1<<2)
static uint32_t gnss_data_flags = 0;
static gnss_struct_t gnss_data_in_progress;
static uint32_t gnss_tow_in_progress = 0;
static uint64_t gnss_unix_timestamp = 0;

static bool gnss_data_is_valid = false;
static gnss_struct_t gnss_data;

static bool gnss_lastfix_is_valid = false;
static gnss_last_fix_struct_t gnss_lastfix;

static struct k_mutex gnss_data_mutex;
static struct k_mutex gnss_cb_mutex;

static gnss_data_cb_t data_cb = NULL;
static gnss_lastfix_cb_t lastfix_cb = NULL;

/**
 *
 */


static int mia_m10_set_uart_baudrate(uint32_t baudrate)
{
	int ret = 0;
	struct uart_config cfg;

	if (baudrate == 0) {
		return -EINVAL;
	}
	ret = uart_config_get(mia_m10_uart_dev, &cfg);
	if (ret != 0) {
		return -EIO;
	}
	if (cfg.baudrate == baudrate) {
		return 0;
	}

	cfg.baudrate = baudrate;
	ret = uart_configure(mia_m10_uart_dev, &cfg);
	if (ret != 0) {
		return -EIO;
	}

	return ret;
}

static int mia_m10_sync_tow(uint32_t tow)
{
	if (tow != gnss_tow_in_progress) {
		/* Mismatching TOW, reset flags and buffers */
		memset(&gnss_data_in_progress, 0, sizeof(gnss_struct_t));
		gnss_data_flags = 0;
		gnss_tow_in_progress = tow;
	}

	return 0;
}

static int mia_m10_sync_complete(uint32_t flag)
{
	gnss_data_flags |= flag;

	if (gnss_data_flags == (GNSS_DATA_FLAG_NAV_DOP|GNSS_DATA_FLAG_NAV_PVT|GNSS_DATA_FLAG_NAV_STATUS)) {

		/* Copy data from "in progress" to "working", and call callbacks */
		if (k_mutex_lock(&gnss_data_mutex, K_MSEC(10)) == 0) {
			memcpy(&gnss_data, &gnss_data_in_progress, sizeof(gnss_struct_t));
			gnss_data.updated_at = k_uptime_get_32();
			gnss_data_is_valid = true;

			if (gnss_data.pvt_flags&1) {
				gnss_lastfix.h_acc_dm = gnss_data.h_acc_dm;
				gnss_lastfix.lat = gnss_data.lat;
				gnss_lastfix.lon = gnss_data.lon;
				gnss_lastfix.unix_timestamp = gnss_unix_timestamp; 

				gnss_lastfix.head_veh = gnss_data.head_veh;
				gnss_lastfix.pvt_flags = gnss_data.pvt_flags;
				gnss_lastfix.head_acc = gnss_data.head_acc;
				gnss_lastfix.h_dop = gnss_data.h_dop;
				gnss_lastfix.num_sv = gnss_data.num_sv;
				gnss_lastfix.height = gnss_data.height;
				/* TODO - Barometer data here?! */
				gnss_lastfix.baro_height = 0;
				
				gnss_lastfix.msss = gnss_data.msss;

				/* TODO - What to do?! */
				gnss_lastfix.gps_mode = 0;

				gnss_lastfix_is_valid = true;
			}
			
			k_mutex_unlock(&gnss_data_mutex);
		}

		if (k_mutex_lock(&gnss_cb_mutex, K_MSEC(10)) == 0) {
			if (data_cb != NULL) {
				data_cb(&gnss_data);
			}
			if (gnss_data.pvt_flags&1) {
				lastfix_cb(&gnss_lastfix);
			}

			k_mutex_unlock(&gnss_cb_mutex);
		} else {
			return -ETIME;
		}
	}

	return 0;
}

static uint64_t mia_m10_nav_pvt_to_unix_time(struct ublox_nav_pvt* nav_pvt)
{
	struct tm now = { 0 };
	/* tm_year is years since 1900 */
	now.tm_year = nav_pvt->year - 1900;
	/* tm_mon allowed values are 0-11 */
	now.tm_mon = nav_pvt->month - 1;
	now.tm_mday = nav_pvt->day;

	now.tm_hour = nav_pvt->hour;
	now.tm_min = nav_pvt->min;
	now.tm_sec = nav_pvt->sec;

	return timeutil_timegm64(&now);
}

static int mia_m10_nav_pvt_handler(void* context, void* payload, uint32_t size)
{
	struct ublox_nav_pvt* nav_pvt = payload;

	mia_m10_sync_tow(nav_pvt->iTOW);

	gnss_data_in_progress.pvt_flags = nav_pvt->flags;
	gnss_data_in_progress.pvt_valid = nav_pvt->valid;
	gnss_data_in_progress.lon = nav_pvt->lon;
	gnss_data_in_progress.lat = nav_pvt->lat;
	gnss_data_in_progress.num_sv = nav_pvt->numSV;
	gnss_data_in_progress.speed = (uint16_t)nav_pvt->gSpeed;
	gnss_data_in_progress.head_veh = (int16_t)(nav_pvt->headVeh/1000);
	gnss_data_in_progress.head_acc = (int16_t)(nav_pvt->headAcc/1000);
	/* Calculate from mm to dm, and within int16 limits */
	gnss_data_in_progress.height = (int16_t) MIN(INT16_MAX, MAX(INT16_MIN, nav_pvt->height/100));
	gnss_data_in_progress.h_acc_dm = (uint16_t) MIN(UINT16_MAX, nav_pvt->hAcc/100);
	gnss_data_in_progress.v_acc_dm = (uint16_t) MIN(UINT16_MAX, nav_pvt->vAcc/100);

	if (((nav_pvt->valid&0x7) == 0x7) && (nav_pvt->flags&1)) {
		gnss_unix_timestamp = mia_m10_nav_pvt_to_unix_time(nav_pvt);
	}

	mia_m10_sync_complete(GNSS_DATA_FLAG_NAV_PVT);

	return 0;
}

int mia_m10_nav_dop_handler(void* context, void* payload, uint32_t size)
{
	struct ublox_nav_dop* nav_dop = payload;

	mia_m10_sync_tow(nav_dop->iTOW);
	
	gnss_data_in_progress.h_dop = nav_dop->hDOP;

	mia_m10_sync_complete(GNSS_DATA_FLAG_NAV_DOP);

	return 0;
}

int mia_m10_nav_status_handler(void* context, void* payload, uint32_t size)
{
	struct ublox_nav_status* nav_status = payload;

	mia_m10_sync_tow(nav_status->iTOW);
	
	gnss_data_in_progress.msss = nav_status->msss;
	gnss_data_in_progress.ttff = nav_status->ttff;

	mia_m10_sync_complete(GNSS_DATA_FLAG_NAV_STATUS);

	return 0;
}

static int mia_m10_setup(const struct device *dev, bool try_default_baud_first)
{
	int ret = 0;

	/* TODO - Could this wait be removed somehow? */
	/* Wait to assure startup */
	k_sleep(K_MSEC(500));

	if (try_default_baud_first) {
		/* Try with default baudrate first */
		ret = mia_m10_set_uart_baudrate(MIA_M10_DEFAULT_BAUDRATE);
		if (ret != 0) {
			return ret;
		}
	}

	/* Check if communication is working by sending dummy command */
	uint32_t gnss_baudrate = 0;
	ret = mia_m10_config_get_u32(UBX_CFG_UART1_BAUDRATE, &gnss_baudrate);
	if (ret != 0) {
		/* Communication failed, try other baudrate */
		gnss_baudrate = try_default_baud_first ? 
				    CONFIG_GNSS_MIA_M10_UART_BAUDRATE : 
				    MIA_M10_DEFAULT_BAUDRATE;
		ret = mia_m10_set_uart_baudrate(gnss_baudrate);
		if (ret != 0) {
			return ret;
		}

		ret = mia_m10_config_get_u32(UBX_CFG_UART1OUTPRO_NMEA, &gnss_baudrate);
		if (ret != 0) {
			return -EIO;
		}
	}

	/* Change UART baudrate if required */
	if (gnss_baudrate != CONFIG_GNSS_MIA_M10_UART_BAUDRATE) {
		/* Requested baudrate change will be performed by ISR on 
		 * completed transaction of command */
		mia_m10_baudrate = CONFIG_GNSS_MIA_M10_UART_BAUDRATE;
		atomic_set_bit(&mia_m10_baudrate_change_req, 0);

		ret = mia_m10_config_set_u32(UBX_CFG_UART1_BAUDRATE, mia_m10_baudrate);
		if (ret != 0) {
			return ret;
		}
	}

	/* Disable NMEA output on UART */
	ret = mia_m10_config_set_u8(UBX_CFG_UART1OUTPRO_NMEA, 0);
	if (ret != 0) {
		return ret;
	}
	/* Enable NAV-PVT output on UART */
	ret = mia_m10_config_set_u8(UBX_CFG_MSGOUT_UBX_NAV_PVT_UART1, 1);
	if (ret != 0) {
		return ret;
	}
	ret = ublox_register_handler(UBX_NAV, UBX_NAV_PVT, 
				     mia_m10_nav_pvt_handler, NULL);
	if (ret != 0) {
		return ret;
	}
	/* Enable NAV-DOP output on UART */
	ret = mia_m10_config_set_u8(UBX_CFG_MSGOUT_UBX_NAV_DOP_UART1, 1);
	if (ret != 0) {
		return ret;
	}
	ret = ublox_register_handler(UBX_NAV, UBX_NAV_DOP, 
				     mia_m10_nav_dop_handler, NULL);
	if (ret != 0) {
		return ret;
	}
	/* Enable NAV-STATUS output on UART */
	ret = mia_m10_config_set_u8(UBX_CFG_MSGOUT_UBX_NAV_STATUS_UART1, 1);
	if (ret != 0) {
		return ret;
	}
	ret = ublox_register_handler(UBX_NAV, UBX_NAV_STATUS, 
				     mia_m10_nav_status_handler, NULL);
	if (ret != 0) {
		return ret;
	}

	return 0;
}

static int mia_m10_reset(const struct device *dev, uint16_t mask, uint8_t mode)
{
	int ret = 0;

	ret = mia_m10_send_reset(mask, mode);
	if (ret != 0) {
		return ret;
	}
	return 0;
}

static int mia_m10_upload_assist_data(const struct device *dev, uint8_t* data, uint32_t size)
{
	int ret = 0;

	if (size != 76) {
		return -EINVAL;
	}

	ret = mia_m10_send_assist_data(data, size);
	if (ret != 0) {
		return ret;
	}
	return 0;
}

static int mia_m10_set_rate(const struct device *dev, uint16_t rate)
{
	int ret = 0;

	if (rate < 25) {
		return -EINVAL;
	}

	ret = mia_m10_config_set_u16(UBX_CFG_RATE_MEAS, rate);
	if (ret != 0) {
		return ret;
	}
	return 0;
}

static int mia_m10_get_rate(const struct device *dev, uint16_t* rate)
{
	int ret = 0;

	ret = mia_m10_config_get_u16(UBX_CFG_RATE_MEAS, rate);
	if (ret != 0) {
		return ret;
	}
	return 0;
}

static int mia_m10_set_data_cb(const struct device *dev, gnss_data_cb_t gnss_data_cb)
{
	if (k_mutex_lock(&gnss_cb_mutex, K_MSEC(1000)) == 0) {
		data_cb = gnss_data_cb;
		k_mutex_unlock(&gnss_cb_mutex);
	} else {
		return -ETIME;
	}
	return 0;
}

static int mia_m10_set_lastfix_cb(const struct device *dev, gnss_lastfix_cb_t gnss_lastfix_cb)
{
	if (k_mutex_lock(&gnss_cb_mutex, K_MSEC(1000)) == 0) {
		lastfix_cb = gnss_lastfix_cb;
		k_mutex_unlock(&gnss_cb_mutex);
	} else {
		return -ETIME;
	}
	return 0;
}

static int mia_m10_data_fetch(const struct device *dev, gnss_struct_t* data)
{
	if (k_mutex_lock(&gnss_data_mutex, K_MSEC(1000)) == 0) {

		if (!gnss_data_is_valid) {
			k_mutex_unlock(&gnss_data_mutex);
			return -ENODATA;
		}

		memcpy(data, &gnss_data, sizeof(gnss_struct_t));

		k_mutex_unlock(&gnss_data_mutex);
	}
	
	return 0;
}

static int mia_m10_lastfix_fetch(const struct device *dev, gnss_last_fix_struct_t* lastfix)
{
	if (k_mutex_lock(&gnss_data_mutex, K_MSEC(1000)) == 0) {

		if (!gnss_lastfix_is_valid) {
			k_mutex_unlock(&gnss_data_mutex);
			return -ENODATA;
		}

		memcpy(lastfix, &gnss_lastfix, sizeof(gnss_last_fix_struct_t));
		
		k_mutex_unlock(&gnss_data_mutex);
	}

	return 0;
}

static const struct gnss_driver_api mia_m10_api_funcs = {
	.gnss_setup = mia_m10_setup,
	.gnss_reset = mia_m10_reset,

	.gnss_upload_assist_data = mia_m10_upload_assist_data,

	.gnss_set_rate = mia_m10_set_rate,
	.gnss_get_rate = mia_m10_get_rate,

	.gnss_set_data_cb = mia_m10_set_data_cb,
	.gnss_set_lastfix_cb = mia_m10_set_lastfix_cb,

	.gnss_data_fetch = mia_m10_data_fetch,
	.gnss_lastfix_fetch = mia_m10_lastfix_fetch
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
}

/**
 * @brief Handles completion of UART transaction. 
 * 
 * @param[in] uart_dev UART device to send to. 
 */
static void mia_m10_uart_handle_complete(const struct device *uart_dev)
{	
	if (ring_buf_is_empty(&gnss_tx_ring_buf)) {
		uart_irq_tx_disable(uart_dev);

		if (atomic_test_and_clear_bit(&mia_m10_baudrate_change_req, 0)) {
			mia_m10_set_uart_baudrate(mia_m10_baudrate);
		}
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

		if (uart_irq_tx_complete(uart_dev)) {
			mia_m10_uart_handle_complete(uart_dev);
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

static int mia_m10_init(const struct device *dev)
{
	mia_m10_uart_dev = GNSS_UART_DEV;

	/* Initialize Ublox protocol parser */
	ublox_protocol_init();
	k_mutex_init(&cmd_mutex);
	k_sem_init(&cmd_ack_sem, 0, 1);
	k_sem_init(&cmd_poll_sem, 0, 1);

	k_mutex_init(&gnss_data_mutex);
	k_mutex_init(&gnss_cb_mutex);

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
	if (wait_for_ack) {
		if (k_sem_take(&cmd_ack_sem, K_MSEC(CONFIG_GNSS_MIA_M10_CMD_RESP_TIMEOUT)) != 0) {
			LOG_ERR("ACK timed out");
			k_mutex_unlock(&cmd_mutex);
			return -ETIME;
		}
	}
	if (wait_for_data) {
		if (k_sem_take(&cmd_poll_sem, K_MSEC(CONFIG_GNSS_MIA_M10_CMD_RESP_TIMEOUT)) != 0) {
			LOG_ERR("POLL timed out");
			k_mutex_unlock(&cmd_mutex);
			return -ETIME;
		}
	}

	return 0;
}

int mia_m10_config_get_u8(uint32_t key, uint8_t* value)
{
	uint64_t raw_value;
	int ret = mia_m10_config_get(key, 1, &raw_value);
	if (ret != 0) {
		return ret;
	}

	*value = raw_value&0xFF;

	return 0;
}

int mia_m10_config_get_u16(uint32_t key, uint16_t* value)
{
	uint64_t raw_value;
	int ret = mia_m10_config_get(key, 2, &raw_value);
	if (ret != 0) {
		return ret;
	}

	*value = raw_value&0xFFFF;

	return 0;
}

int mia_m10_config_get_u32(uint32_t key, uint32_t* value)
{
	uint64_t raw_value;
	int ret = mia_m10_config_get(key, 4, &raw_value);
	if (ret != 0) {
		return ret;
	}

	*value = raw_value&0xFFFF;

	return 0;
}

int mia_m10_config_get_f64(uint32_t key, double* value)
{
	uint64_t raw_value;
	int ret = mia_m10_config_get(key, 8, &raw_value);
	if (ret != 0) {
		return ret;
	}

	double* val = (void*)&raw_value;
	*value = *val;

	return 0;
}

int mia_m10_config_get(uint32_t key, uint8_t size, uint64_t* raw_value)
{
	int ret = 0;

	if (k_mutex_lock(&cmd_mutex, K_MSEC(CONFIG_GNSS_MIA_M10_CMD_RESP_TIMEOUT)) == 0) {

		ret = ublox_build_cfg_valget(cmd_buf, &cmd_size, CONFIG_GNSS_MIA_M10_CMD_MAX_SIZE,
					     0, 0, 
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

		/* Check ACK */
		if (!cmd_ack) {
			k_mutex_unlock(&cmd_mutex);
			return -ECONNREFUSED;
		}

		/* Parse result payload */
		ret = ublox_get_cfg_val(cmd_buf, cmd_size, size, raw_value);
		if (ret != 0) {
			k_mutex_unlock(&cmd_mutex);
			return ret;
		}

		k_mutex_unlock(&cmd_mutex);
	} else {
		return -EBUSY;
	}

	return ret;
}

int mia_m10_config_set_u8(uint32_t key, uint8_t value)
{
	uint64_t raw_value = value;
	return mia_m10_config_set(key, raw_value);
}

int mia_m10_config_set_u16(uint32_t key, uint16_t value)
{
	uint64_t raw_value = value;
	return mia_m10_config_set(key, raw_value);
}

int mia_m10_config_set_u32(uint32_t key, uint32_t value)
{
	uint64_t raw_value = value;
	return mia_m10_config_set(key, raw_value);
}

int mia_m10_config_set(uint32_t key, uint64_t raw_value)
{
	int ret = 0;

	if (k_mutex_lock(&cmd_mutex, K_MSEC(CONFIG_GNSS_MIA_M10_CMD_RESP_TIMEOUT)) == 0) {

		ret = ublox_build_cfg_valset(cmd_buf, &cmd_size, CONFIG_GNSS_MIA_M10_CMD_MAX_SIZE,
					     3, 
					     key, raw_value);
		if (ret != 0) {
			k_mutex_unlock(&cmd_mutex);
			return ret;
		}

		ret = mia_m10_send_ubx_cmd(cmd_buf, cmd_size, true, false);
		if (ret != 0) {
			k_mutex_unlock(&cmd_mutex);
			return ret;
		}

		/* Check ACK */
		if (!cmd_ack) {
			ret = -ECONNREFUSED;
		}

		k_mutex_unlock(&cmd_mutex);
	} else {
		return -EBUSY;
	}

	return ret;
}

int mia_m10_send_reset(uint16_t mask, uint8_t mode)
{
	int ret = 0;

	if (k_mutex_lock(&cmd_mutex, K_MSEC(CONFIG_GNSS_MIA_M10_CMD_RESP_TIMEOUT)) == 0) {

		ret = ublox_build_cfg_rst(cmd_buf, &cmd_size, CONFIG_GNSS_MIA_M10_CMD_MAX_SIZE,
					  mask, mode);
		if (ret != 0) {
			k_mutex_unlock(&cmd_mutex);
			return ret;
		}

		ret = mia_m10_send_ubx_cmd(cmd_buf, cmd_size, false, false);
		if (ret != 0) {
			k_mutex_unlock(&cmd_mutex);
			return ret;
		}

		k_mutex_unlock(&cmd_mutex);
	} else {
		return -EBUSY;
	}

	return ret;
}

int mia_m10_send_assist_data(uint8_t* data, uint32_t size)
{
	int ret = 0;

	if (k_mutex_lock(&cmd_mutex, K_MSEC(CONFIG_GNSS_MIA_M10_CMD_RESP_TIMEOUT)) == 0) {

		ret = ublox_build_mga_ano(cmd_buf, &cmd_size, CONFIG_GNSS_MIA_M10_CMD_MAX_SIZE,
					  data, size);
		if (ret != 0) {
			k_mutex_unlock(&cmd_mutex);
			return ret;
		}

		ret = mia_m10_send_ubx_cmd(cmd_buf, cmd_size, true, false);
		if (ret != 0) {
			k_mutex_unlock(&cmd_mutex);
			return ret;
		}

		k_mutex_unlock(&cmd_mutex);
	} else {
		return -EBUSY;
	}

	return ret;
}

static struct mia_m10_dev_data mia_m10_data;

static const struct mia_m10_dev_config mia_m10_config = {
	.uart_name = DT_INST_BUS_LABEL(0),
};

DEVICE_DT_INST_DEFINE(0, mia_m10_init, NULL,
		    &mia_m10_data, &mia_m10_config, POST_KERNEL,
		    CONFIG_GNSS_INIT_PRIORITY, &mia_m10_api_funcs);
