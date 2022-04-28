#define DT_DRV_COMPAT u_blox_mia_m10

#include "ublox-mia-m10.h"

#include <init.h>
#include <zephyr.h>
#include <sys/ring_buffer.h>
#include <drivers/gpio.h>
#include <logging/log.h>
#include <sys/timeutil.h>

#include "gnss.h"
#include "gnss_uart.h"
#include "ublox_protocol.h"
#include "nmea_parser.h"

LOG_MODULE_REGISTER(MIA_M10, CONFIG_GNSS_LOG_LEVEL);

/* GNSS UART device */
#define GNSS_UART_NODE DT_INST_BUS(0)
#define GNSS_UART_DEV DEVICE_DT_GET(GNSS_UART_NODE)

static const struct device *mia_m10_uart_dev = GNSS_UART_DEV;

#define MIA_M10_DEFAULT_BAUDRATE 38400

/* Parser thread structures */
K_KERNEL_STACK_DEFINE(gnss_parse_stack, CONFIG_GNSS_MIA_M10_PARSE_STACK_SIZE);
struct k_thread gnss_parse_thread;

/* Command / Response control structures and buffer. 
 * Buffer is used for building command before sending, and will also be 
 * populated with the response data when applicable.
 */
static struct k_mutex cmd_mutex;
static struct k_sem cmd_ack_sem;
static struct k_sem cmd_data_sem;
static bool cmd_ack = false;
static uint8_t cmd_buf[CONFIG_GNSS_MIA_M10_CMD_MAX_SIZE];
static uint32_t cmd_size = 0;

/* ANO data ack result */
static struct k_sem mga_ack_sem;
static bool mga_ack = false;

/* Semaphore for signalling thread about received data. */
struct k_sem gnss_rx_sem;

/* GNSS data is aggregated from three messages; NAV-PVT, NAV-DOP and 
 * NAV-STATUS. The time of week in the messages is used to find what
 * data belongs together. When a new TOW is encountered, the flags 
 * and buffers are reset. Data is copied and flag is set for each
 * incoming message. When all flags are set, a new GNSS data packet
 * is ready for use. 
 */
#define GNSS_DATA_FLAG_NAV_DOP (1 << 0)
#define GNSS_DATA_FLAG_NAV_PVT (1 << 1)
#define GNSS_DATA_FLAG_NAV_STATUS (1 << 2)

static uint32_t gnss_data_flags = 0;
static gnss_struct_t gnss_data_in_progress;
static uint32_t gnss_tow_in_progress = 0;
static uint64_t gnss_unix_timestamp = 0;

static gnss_t gnss_data;
bool gnss_data_is_valid = false;

static struct k_mutex gnss_data_mutex;
static struct k_mutex gnss_cb_mutex;

static gnss_data_cb_t data_cb = NULL;

/**
 * @brief Synchronizes on time of week (TOW). If a new TOW is encountered, 
 *        flags and buffers are cleared. 
 *
 * @param[in] tow Time of week from GNSS data.
 * 
 * @return 0 if everything was ok, error code otherwise
 */
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

/**
 * @brief Register new piece of the GNSS data, (PVT, DOP or STATUS). 
 *        Data is signalled as ready for use when all pieces are registered. 
 *
 * @param[in] flag Flag of current piece of data. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static int mia_m10_sync_complete(uint32_t flag)
{
	gnss_data_flags |= flag;

	if (gnss_data_flags ==
	    (GNSS_DATA_FLAG_NAV_DOP | GNSS_DATA_FLAG_NAV_PVT |
	     GNSS_DATA_FLAG_NAV_STATUS)) {
		/* Copy data from "in progress" to "working", and call callbacks */
		if (k_mutex_lock(&gnss_data_mutex, K_MSEC(10)) == 0) {
			memcpy(&gnss_data.latest, &gnss_data_in_progress,
			       sizeof(gnss_struct_t));
			gnss_data.latest.updated_at = k_uptime_get_32();
			gnss_data_is_valid = true;
			gnss_data.fix_ok =
				(gnss_data.latest.pvt_flags & 1) != 0;

			if (gnss_data.fix_ok) {
				gnss_data.lastfix.h_acc_dm =
					gnss_data.latest.h_acc_dm;
				gnss_data.lastfix.lat = gnss_data.latest.lat;
				gnss_data.lastfix.lon = gnss_data.latest.lon;
				gnss_data.lastfix.unix_timestamp =
					gnss_unix_timestamp;

				gnss_data.lastfix.head_veh =
					gnss_data.latest.head_veh;
				gnss_data.lastfix.pvt_flags =
					gnss_data.latest.pvt_flags;
				gnss_data.lastfix.head_acc =
					gnss_data.latest.head_acc;
				gnss_data.lastfix.h_dop =
					gnss_data.latest.h_dop;
				gnss_data.lastfix.num_sv =
					gnss_data.latest.num_sv;
				gnss_data.lastfix.height =
					gnss_data.latest.height;

				gnss_data.lastfix.msss = gnss_data.latest.msss;

				/* TODO - What to do?! */
				gnss_data.lastfix.gps_mode = 0;

				gnss_data.has_lastfix = true;
			}

			k_mutex_unlock(&gnss_data_mutex);
		}

		if (k_mutex_lock(&gnss_cb_mutex, K_MSEC(10)) == 0) {
			if (data_cb != NULL) {
				data_cb(&gnss_data);
			}

			k_mutex_unlock(&gnss_cb_mutex);
		} else {
			return -ETIME;
		}
	}

	return 0;
}

/**
 * @brief Convert date from NAV-PVT message to unix epoch time. 
 *
 * @param[in] nav_pvt NAV-PVT message.
 * 
 * @return Unix epoch time.
 */
static uint64_t mia_m10_nav_pvt_to_unix_time(struct ublox_nav_pvt *nav_pvt)
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

/**
 * @brief Handler for incoming NAV-PVT message. 
 *
 * @param[in] context Context is unused. 
 * @param[in] payload Payload containing NAV-PVT data. 
 * @param[in] size Size of payload. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static int mia_m10_nav_pvt_handler(void *context, void *payload, uint32_t size)
{
	struct ublox_nav_pvt *nav_pvt = payload;

	mia_m10_sync_tow(nav_pvt->iTOW);

	gnss_data_in_progress.pvt_flags = nav_pvt->flags;
	gnss_data_in_progress.pvt_valid = nav_pvt->valid;
	gnss_data_in_progress.lon = nav_pvt->lon;
	gnss_data_in_progress.lat = nav_pvt->lat;
	gnss_data_in_progress.num_sv = nav_pvt->numSV;
	gnss_data_in_progress.speed = (uint16_t)nav_pvt->gSpeed;
	gnss_data_in_progress.head_veh = (int16_t)(nav_pvt->headVeh / 1000);
	gnss_data_in_progress.head_acc = (int16_t)(nav_pvt->headAcc / 1000);
	/* Calculate from mm to dm, and within int16 limits */
	gnss_data_in_progress.height =
		(int16_t)MIN(INT16_MAX, MAX(INT16_MIN, nav_pvt->height / 100));
	gnss_data_in_progress.h_acc_dm =
		(uint16_t)MIN(UINT16_MAX, nav_pvt->hAcc / 100);
	gnss_data_in_progress.v_acc_dm =
		(uint16_t)MIN(UINT16_MAX, nav_pvt->vAcc / 100);

	if (((nav_pvt->valid & 0x7) == 0x7) && (nav_pvt->flags & 1)) {
		gnss_unix_timestamp = mia_m10_nav_pvt_to_unix_time(nav_pvt);
	}

	mia_m10_sync_complete(GNSS_DATA_FLAG_NAV_PVT);

	return 0;
}

/**
 * @brief Handler for incoming NAV-DOP message. 
 *
 * @param[in] context Context is unused. 
 * @param[in] payload Payload containing NAV-DOP data. 
 * @param[in] size Size of payload. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static int mia_m10_nav_dop_handler(void *context, void *payload, uint32_t size)
{
	struct ublox_nav_dop *nav_dop = payload;

	mia_m10_sync_tow(nav_dop->iTOW);

	gnss_data_in_progress.h_dop = nav_dop->hDOP;

	mia_m10_sync_complete(GNSS_DATA_FLAG_NAV_DOP);

	return 0;
}

/**
 * @brief Handler for incoming NAV-STATUS message. 
 *
 * @param[in] context Context is unused. 
 * @param[in] payload Payload containing NAV-STATUS data. 
 * @param[in] size Size of payload. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static int mia_m10_nav_status_handler(void *context, void *payload, uint32_t size)
{
	struct ublox_nav_status *nav_status = payload;

	mia_m10_sync_tow(nav_status->iTOW);

	gnss_data_in_progress.msss = nav_status->msss;
	gnss_data_in_progress.ttff = nav_status->ttff;

	mia_m10_sync_complete(GNSS_DATA_FLAG_NAV_STATUS);

	return 0;
}

/**
 * @brief Handler for incoming MGA-ACK message. 
 *
 * @param[in] context Context is unused. 
 * @param[in] payload Payload containing MGA-ACK data. 
 * @param[in] size Size of payload. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static int mia_m10_mga_ack_handler(void *context, void *payload, uint32_t size)
{
	struct ublox_mga_ack *mga_ack_msg = payload;

	/* Check if receiver accepted data */
	mga_ack = (mga_ack_msg->infoCode == 0);
	k_sem_give(&mga_ack_sem);

	return 0;
}

/**
 * @brief Perform setup of GNSS. 
 *
 * @param[in] dev Device context. 
 * @param[in] try_default_baud_first Set baudrate to default for GNSS. Before 
 *                                   performing setup.
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static int mia_m10_setup(const struct device *dev, bool try_default_baud_first)
{
	ARG_UNUSED(dev);
	int ret = 0;

	/* Clear GNSS data */
	gnss_data_is_valid = false;
	memset(&gnss_data, 0, sizeof(gnss_t));

	/* TODO - Could this wait be removed somehow? */
	/* Wait to assure startup */
	k_sleep(K_MSEC(500));

	if (try_default_baud_first) {
		/* Try with default baudrate first */
		ret = gnss_uart_set_baudrate(MIA_M10_DEFAULT_BAUDRATE, true);
		if (ret != 0) {
			return ret;
		}
	}

	/* Check if communication is working by sending dummy command */
	uint32_t gnss_baudrate = gnss_uart_get_baudrate();
	ret = mia_m10_config_get_u32(UBX_CFG_UART1_BAUDRATE, &gnss_baudrate);
	if (ret != 0) {
		/* Communication failed, try other baudrate */
		gnss_baudrate = (gnss_baudrate == MIA_M10_DEFAULT_BAUDRATE) ?
					CONFIG_GNSS_MIA_M10_UART_BAUDRATE :
					MIA_M10_DEFAULT_BAUDRATE;
		ret = gnss_uart_set_baudrate(gnss_baudrate, true);
		if (ret != 0) {
			return ret;
		}

		ret = mia_m10_config_get_u32(UBX_CFG_UART1_BAUDRATE,
					     &gnss_baudrate);
		if (ret != 0) {
			return -EIO;
		}
	}

	/* Change UART baudrate if required */
	if (gnss_baudrate != CONFIG_GNSS_MIA_M10_UART_BAUDRATE) {
		gnss_uart_set_baudrate(CONFIG_GNSS_MIA_M10_UART_BAUDRATE,
				       false);

		ret = mia_m10_config_set_u32(UBX_CFG_UART1_BAUDRATE,
					     CONFIG_GNSS_MIA_M10_UART_BAUDRATE);
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

	/* Enable NAV-SAT output on UART, no handler */
	ret = mia_m10_config_set_u8(UBX_CFG_MSGOUT_UBX_NAV_SAT_UART1, 1);
	if (ret != 0) {
		return ret;
	}

	/* Enable MGA-ACK output on UART */
	ret = mia_m10_config_set_u8(UBX_CFG_NAVSPG_ACKAIDING, 1);
	if (ret != 0) {
		return ret;
	}
	ret = ublox_register_handler(UBX_MGA, UBX_MGA_ACK,
				     mia_m10_mga_ack_handler, NULL);
	if (ret != 0) {
		return ret;
	}

	return 0;
}

/**
 * @brief Resets GNSS. See GNSS API definition for details.
 *
 * @param[in] dev Device context. 
 * @param[in] mask Mask of what to reset. 
 * @param[in] mode Mode of reset. E.g. HW/SW. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static int mia_m10_reset(const struct device *dev, uint16_t mask, uint8_t mode)
{
	ARG_UNUSED(dev);
	int ret = 0;

	ret = mia_m10_send_reset(mask, mode);
	if (ret != 0) {
		return ret;
	}
	return 0;
}

/**
 * @brief Uploads assistance data to GNSS. 
 *
 * @param[in] dev Device context. 
 * @param[in] data Buffer of assistance data. 
 * @param[in] size Size of data.
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static int mia_m10_upload_assist_data(const struct device *dev, uint8_t *data,
				      uint32_t size)
{
	ARG_UNUSED(dev);
	int ret = 0;

	/* Size of assist data must be 76 Bytes according to datasheet */
	if (size != 76) {
		return -EINVAL;
	}

	ret = mia_m10_send_assist_data(data, size);
	if (ret != 0) {
		return ret;
	}
	return 0;
}

/**
 * @brief Set data rate of GNSS.
 *
 * @param[in] dev Device context. 
 * @param[in] rate Rate to use. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static int mia_m10_set_rate(const struct device *dev, uint16_t rate)
{
	ARG_UNUSED(dev);
	int ret = 0;

	/* According to datasheet, rate should be minimum 25ms */
	if (rate < 25) {
		return -EINVAL;
	}

	ret = mia_m10_config_set_u16(UBX_CFG_RATE_MEAS, rate);
	if (ret != 0) {
		return ret;
	}
	return 0;
}

/**
 * @brief Get data rate of GNSS. 
 *
 * @param[in] dev Device context. 
 * @param[out] rate Rate in use. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static int mia_m10_get_rate(const struct device *dev, uint16_t *rate)
{
	ARG_UNUSED(dev);
	int ret = 0;

	ret = mia_m10_config_get_u16(UBX_CFG_RATE_MEAS, rate);
	if (ret != 0) {
		return ret;
	}
	return 0;
}

/**
 * @brief Set callback to call when new GNSS data has been received. 
 *
 * @param[in] dev Device context. 
 * @param[in] gnss_data_cb Function to call upon receiving GNSS data. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static int mia_m10_set_data_cb(const struct device *dev,
			       gnss_data_cb_t gnss_data_cb)
{
	ARG_UNUSED(dev);
	if (k_mutex_lock(&gnss_cb_mutex, K_MSEC(1000)) == 0) {
		data_cb = gnss_data_cb;
		k_mutex_unlock(&gnss_cb_mutex);
	} else {
		return -ETIME;
	}
	return 0;
}

/**
 * @brief Get GNSS data. 
 *
 * @param[in] dev Device context. 
 * @param[out] data GNSS data. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static int mia_m10_data_fetch(const struct device *dev, gnss_t *data)
{
	ARG_UNUSED(dev);
	if (k_mutex_lock(&gnss_data_mutex, K_MSEC(1000)) == 0) {
		if (!gnss_data_is_valid) {
			k_mutex_unlock(&gnss_data_mutex);
			return -ENODATA;
		}

		memcpy(data, &gnss_data, sizeof(gnss_t));

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

	.gnss_data_fetch = mia_m10_data_fetch,
};

/**
 * @brief Parse data from GNSS. 
 *
 * @param[in] buffer Buffer with data. 
 * @param[in] cnt Count of bytes in buffer. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static uint32_t mia_m10_parse_data(uint8_t *buffer, uint32_t cnt)
{
	uint32_t parsed = 0;
	while ((cnt - parsed) > 0) {
		if (buffer[parsed] == NMEA_START_DELIMITER) {
			/* NMEA */
			uint32_t i =
				nmea_parse(&buffer[parsed], (cnt - parsed));
			if (i == 0) {
				/* Nothing parsed means not enough data yet */
				break;
			} else {
				parsed += i;
			}
		} else if (buffer[parsed] == UBLOX_SYNC_CHAR_1) {
			/* UBLOX */
			uint32_t i =
				ublox_parse(&buffer[parsed], (cnt - parsed));
			if (i == 0) {
				/* Nothing parsed means not enough data yet */
				break;
			} else {
				parsed += i;
			}
		} else {
			parsed++;
		}
	}

	return parsed;
}

/**
 * @brief Thread function for handling received data. 
 *
 * @param[in] dev Device context. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static void mia_m10_handle_received_data(void *dev)
{
	bool is_parsing;
	uint32_t total_parsed_cnt;

	while (true) {
		k_sem_take(&gnss_rx_sem, K_FOREVER);

		uint8_t *data_buffer;
		uint32_t data_cnt;
		gnss_uart_rx_get_data(&data_buffer, &data_cnt);

		total_parsed_cnt = 0;
		is_parsing = true;
		while (is_parsing) {
			uint32_t parsed_cnt = mia_m10_parse_data(
				&data_buffer[total_parsed_cnt],
				data_cnt - total_parsed_cnt);

			if (parsed_cnt == 0) {
				is_parsing = false;
			}

			total_parsed_cnt += parsed_cnt;
		}

		gnss_uart_rx_consume(total_parsed_cnt);

		k_yield();
	}
}

/**
 * @brief Initialize driver. 
 *
 * @param[in] dev Device context. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static int mia_m10_init(const struct device *dev)
{
	mia_m10_uart_dev = GNSS_UART_DEV;

	/* Initialize Ublox protocol parser */
	ublox_protocol_init();
	k_mutex_init(&cmd_mutex);
	k_sem_init(&cmd_ack_sem, 0, 1);
	k_sem_init(&cmd_data_sem, 0, 1);
	k_sem_init(&mga_ack_sem, 0, 1);

	k_mutex_init(&gnss_data_mutex);
	k_mutex_init(&gnss_cb_mutex);

	/* Check that UART device is available */
	if (mia_m10_uart_dev == NULL) {
		return -EIO;
	}
	if (!device_is_ready(mia_m10_uart_dev)) {
		return -EIO;
	}

	k_sem_init(&gnss_rx_sem, 0, 1);
	gnss_uart_init(mia_m10_uart_dev, &gnss_rx_sem,
		       MIA_M10_DEFAULT_BAUDRATE);

	/* Start parser thread */
	k_thread_create(&gnss_parse_thread, gnss_parse_stack,
			K_KERNEL_STACK_SIZEOF(gnss_parse_stack),
			(k_thread_entry_t)mia_m10_handle_received_data,
			(void *)mia_m10_uart_dev, NULL, NULL,
			K_PRIO_COOP(CONFIG_GNSS_MIA_M10_THREAD_PRIORITY), 0,
			K_NO_WAIT);

	return 0;
}

/**
 * @brief Callback from protocol on receiving response data. 
 *
 * @param[in] context Context for callback. Unused.
 * @param[in] msg_class Class of message. 
 * @param[in] msg_id ID of message. 
 * @param[in] payload Payload of response. 
 * @param[in] length Payload length.
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static int mia_m10_resp_data_cb(void *context, uint8_t msg_class,
				uint8_t msg_id, void *payload, uint32_t length)
{
	if (length <= sizeof(cmd_buf)) {
		cmd_size = length;
		if (payload != NULL) {
			memcpy(cmd_buf, payload, length);
		}
		k_sem_give(&cmd_data_sem);
	}

	return 0;
}

/**
 * @brief Callback from protocol on receiving response ack/nak. 
 *
 * @param[in] context Context for callback. Unused.
 * @param[in] msg_class Class of message. 
 * @param[in] msg_id ID of message. 
 * @param[in] ack True when ack, false when nak. 
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static int mia_m10_resp_ack_cb(void *context, uint8_t msg_class, uint8_t msg_id,
			       bool ack)
{
	cmd_ack = ack;
	k_sem_give(&cmd_ack_sem);

	return 0;
}

/**
 * @brief Send U-blox protocol command. 
 *
 * @param[in] buffer Buffer with command to send.
 * @param[in] size Size of buffer to send. 
 * @param[in] wait_for_ack True when ack is expected. 
 * @param[in] wait_for_data True when data is expected.
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static int mia_m10_send_ubx_cmd(uint8_t *buffer, uint32_t size,
				bool wait_for_ack, bool wait_for_data)
{
	int ret = 0;

	/* Reset command response parameters */
	k_sem_reset(&cmd_ack_sem);
	k_sem_reset(&cmd_data_sem);
	ublox_set_response_handlers(buffer, mia_m10_resp_data_cb,
				    mia_m10_resp_ack_cb, NULL);

	/* Send command bytes */
	ret = gnss_uart_send(buffer, size);
	if (ret != 0) {
		return ret;
	}

	/* Wait for response, data and ack */
	if (wait_for_ack) {
		if (k_sem_take(&cmd_ack_sem,
			       K_MSEC(CONFIG_GNSS_MIA_M10_CMD_RESP_TIMEOUT)) !=
		    0) {
			LOG_ERR("ACK timed out");
			k_mutex_unlock(&cmd_mutex);
			return -ETIME;
		}
	}
	if (wait_for_data) {
		if (k_sem_take(&cmd_data_sem,
			       K_MSEC(CONFIG_GNSS_MIA_M10_CMD_RESP_TIMEOUT)) !=
		    0) {
			LOG_ERR("DATA timed out");
			k_mutex_unlock(&cmd_mutex);
			return -ETIME;
		}
	}

	return 0;
}

int mia_m10_config_get_u8(uint32_t key, uint8_t *value)
{
	uint64_t raw_value;
	int ret = mia_m10_config_get(key, 1, &raw_value);
	if (ret != 0) {
		return ret;
	}

	*value = raw_value & 0xFF;

	return 0;
}

int mia_m10_config_get_u16(uint32_t key, uint16_t *value)
{
	uint64_t raw_value;
	int ret = mia_m10_config_get(key, 2, &raw_value);
	if (ret != 0) {
		return ret;
	}

	*value = raw_value & 0xFFFF;

	return 0;
}

int mia_m10_config_get_u32(uint32_t key, uint32_t *value)
{
	uint64_t raw_value;
	int ret = mia_m10_config_get(key, 4, &raw_value);
	if (ret != 0) {
		return ret;
	}

	*value = raw_value & 0xFFFFFFFF;

	return 0;
}

int mia_m10_config_get_f64(uint32_t key, double *value)
{
	uint64_t raw_value;
	int ret = mia_m10_config_get(key, 8, &raw_value);
	if (ret != 0) {
		return ret;
	}

	double *val = (void *)&raw_value;
	*value = *val;

	return 0;
}

int mia_m10_config_get(uint32_t key, uint8_t size, uint64_t *raw_value)
{
	int ret = 0;

	if (k_mutex_lock(&cmd_mutex,
			 K_MSEC(CONFIG_GNSS_MIA_M10_CMD_RESP_TIMEOUT)) == 0) {
		ret = ublox_build_cfg_valget(cmd_buf, &cmd_size,
					     CONFIG_GNSS_MIA_M10_CMD_MAX_SIZE,
					     0, 0, key);
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

	if (k_mutex_lock(&cmd_mutex,
			 K_MSEC(CONFIG_GNSS_MIA_M10_CMD_RESP_TIMEOUT)) == 0) {
		ret = ublox_build_cfg_valset(cmd_buf, &cmd_size,
					     CONFIG_GNSS_MIA_M10_CMD_MAX_SIZE,
					     3, key, raw_value);
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

	if (k_mutex_lock(&cmd_mutex,
			 K_MSEC(CONFIG_GNSS_MIA_M10_CMD_RESP_TIMEOUT)) == 0) {
		ret = ublox_build_cfg_rst(cmd_buf, &cmd_size,
					  CONFIG_GNSS_MIA_M10_CMD_MAX_SIZE,
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

int mia_m10_send_assist_data(uint8_t *data, uint32_t size)
{
	int ret = 0;

	if (k_mutex_lock(&cmd_mutex,
			 K_MSEC(CONFIG_GNSS_MIA_M10_CMD_RESP_TIMEOUT)) == 0) {
		ret = ublox_build_mga_ano(cmd_buf, &cmd_size,
					  CONFIG_GNSS_MIA_M10_CMD_MAX_SIZE,
					  data, size);
		if (ret != 0) {
			k_mutex_unlock(&cmd_mutex);
			return ret;
		}

		/* Reset mga response parameters */
		k_sem_reset(&mga_ack_sem);

		ret = mia_m10_send_ubx_cmd(cmd_buf, cmd_size, false, false);
		if (ret != 0) {
			k_mutex_unlock(&cmd_mutex);
			return ret;
		}

		/* Wait for ack */
		if (k_sem_take(&mga_ack_sem,
			       K_MSEC(CONFIG_GNSS_MIA_M10_CMD_RESP_TIMEOUT)) !=
		    0) {
			LOG_ERR("MGA-ACK timed out");
			k_mutex_unlock(&cmd_mutex);
			return -ETIME;
		}

		if (!mga_ack) {
			k_mutex_unlock(&cmd_mutex);
			return -ECONNREFUSED;
		}

		k_mutex_unlock(&cmd_mutex);
	} else {
		return -EBUSY;
	}

	return ret;
}

/* Create device object. 
 * Using NULL for data and config pointers since this is allocated statically 
 * and not in use. 
 */
DEVICE_DT_INST_DEFINE(0, mia_m10_init, NULL, NULL, NULL, POST_KERNEL,
		      CONFIG_GNSS_INIT_PRIORITY, &mia_m10_api_funcs);
