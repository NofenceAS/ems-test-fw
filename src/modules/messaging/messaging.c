/*
 * Copyright (c) 2021 Nofence AS
 */

#include <zephyr.h>
#include <stdlib.h>
#include <string.h>
#include "messaging_module_events.h"
#include "messaging.h"
#include <logging/log.h>
#include "ble_ctrl_event.h"
#include "ble_data_event.h"
#include "ble_cmd_event.h"
#include "nofence_service.h"
#include "lte_proto_event.h"
#include "pwr_event.h"
#include "env_sensor_event.h"

#include "watchdog_event.h"
#include "cellular_controller_events.h"
#include "gnss_controller_events.h"
#include "request_events.h"
#include "nf_version.h"
#include "collar_protocol.h"
#include "UBX.h"
#include "unixTime.h"
#include "error_event.h"
#include "helpers.h"

#include "nf_crc16.h"

#include "storage_event.h"

#include <date_time.h>
#include <time.h>

#include "storage.h"

#include "movement_events.h"

#include "pasture_structure.h"
#include "fw_upgrade_events.h"
#include "sound_event.h"
#include "nf_settings.h"
#include "histogram_events.h"

#define DOWNLOAD_COMPLETE 255
#define GPS_UBX_NAV_PVT_VALID_HEADVEH_MASK 0x20
#define SECONDS_IN_THREE_DAYS 259200
#define TWO_DAYS_SEC SEC_DAY * 2

#define BYTESWAP16(x) (((x) << 8) | ((x) >> 8))

#define EMPTY_FENCE_CRC 0xFFFF
static pasture_t pasture_temp;
static uint8_t cached_fences_counter = 0;

/** @todo This should be fetched from EEPROM */
static gnss_mode_t cached_gnss_mode = GNSSMODE_NOMODE;

uint32_t time_from_server;
atomic_t cached_batt = ATOMIC_INIT(0);
atomic_t cached_chrg = ATOMIC_INIT(0);
atomic_t cached_temp = ATOMIC_INIT(0);
atomic_t cached_press = ATOMIC_INIT(0);
atomic_t cached_hum = ATOMIC_INIT(0);

K_SEM_DEFINE(cache_lock_sem, 1, 1);
K_SEM_DEFINE(send_out_ack, 0, 1);
K_SEM_DEFINE(connection_ready, 0, 1);

collar_state_struct_t current_state;
gnss_last_fix_struct_t cached_fix;

static uint32_t new_fence_in_progress;
static uint8_t expected_fframe, expected_ano_frame, new_ano_in_progress;
static bool first_frame, first_ano_frame;

/* Time since the server updated the date time in seconds. */
static atomic_t server_timestamp_sec = ATOMIC_INIT(0);

uint32_t serial_id = 0;

void build_poll_request(NofenceMessage *);
int8_t request_fframe(uint32_t, uint8_t);
void fence_download(uint8_t);
int8_t request_ano_frame(uint16_t, uint16_t);
void ano_download(uint16_t, uint16_t);
void proto_InitHeader(NofenceMessage *);
void process_poll_response(NofenceMessage *);
void process_upgrade_request(VersionInfoFW *);
uint8_t process_fence_msg(FenceDefinitionResponse *);
uint8_t process_ano_msg(UbxAnoReply *);

int encode_and_send_message(NofenceMessage *);
int send_binary_message(uint8_t *, size_t);

void messaging_thread_fn(void);

static void proto_get_last_known_date_pos(gnss_last_fix_struct_t *gpsLastFix,
					  _DatePos *pos);
bool proto_has_last_known_date_pos(const gnss_last_fix_struct_t *);
static uint32_t ano_date_to_unixtime_midday(uint8_t, uint8_t, uint8_t);
bool m_confirm_acc_limits, m_confirm_ble_key, m_transfer_boot_params;
bool use_server_time;

K_MUTEX_DEFINE(send_binary_mutex);

#define MODULE messaging
LOG_MODULE_REGISTER(MODULE, CONFIG_MESSAGING_LOG_LEVEL);

/* 4 means 4-byte alignment. */
K_MSGQ_DEFINE(ble_ctrl_msgq, sizeof(struct ble_ctrl_event),
	      CONFIG_MSGQ_BLE_CTRL_SIZE, 4);
K_MSGQ_DEFINE(ble_data_msgq, sizeof(struct ble_data_event),
	      CONFIG_MSGQ_BLE_DATA_SIZE, 4);
K_MSGQ_DEFINE(ble_cmd_msgq, sizeof(struct ble_cmd_event),
	      CONFIG_MSGQ_BLE_CMD_SIZE, 4);
K_MSGQ_DEFINE(lte_proto_msgq, sizeof(struct lte_proto_event),
	      CONFIG_MSGQ_LTE_PROTO_SIZE, 4);

#define NUM_MSGQ_EVENTS 4
struct k_poll_event msgq_events[NUM_MSGQ_EVENTS] = {
	K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
					K_POLL_MODE_NOTIFY_ONLY, &ble_ctrl_msgq,
					0),
	K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
					K_POLL_MODE_NOTIFY_ONLY, &ble_data_msgq,
					0),
	K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
					K_POLL_MODE_NOTIFY_ONLY, &ble_cmd_msgq,
					0),
	K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
					K_POLL_MODE_NOTIFY_ONLY,
					&lte_proto_msgq, 0),
};

static struct k_work_q send_q;
struct k_work_delayable modem_poll_work;
struct k_work_delayable log_work;
struct k_work_delayable data_request_work;
struct k_work_delayable process_escape_work;
struct k_work_delayable process_zap_work;
struct k_work_delayable process_warning_work;
struct k_work_delayable process_warning_correction_start_work;
struct k_work_delayable process_warning_correction_end_work;

atomic_t poll_period_minutes = ATOMIC_INIT(5);
atomic_t log_period_minutes = ATOMIC_INIT(30);

K_THREAD_DEFINE(messaging_thread, CONFIG_MESSAGING_THREAD_STACK_SIZE,
		messaging_thread_fn, NULL, NULL, NULL,
		CONFIG_MESSAGING_THREAD_PRIORITY, 0, 0);

K_KERNEL_STACK_DEFINE(messaging_send_thread,
		      CONFIG_MESSAGING_SEND_THREAD_STACK_SIZE);

/**
 * @brief Build the log message with latest data.
 */
static void build_log_message()
{
	/* Fetch histogram data */
	struct save_histogram *histogram_snapshot = new_save_histogram();
	EVENT_SUBMIT(histogram_snapshot);
	collar_histogram histogram;
	int err = k_msgq_get(&histogram_msgq, &histogram, K_SECONDS(10));
	if (err) {
		LOG_ERR("Timeout on waiting for histogram %i", err);
		return;
	}

	/* Fill in NofenceMessage struct */
	NofenceMessage seq_1;
	NofenceMessage seq_2;
	/** @todo also include gprs_stat msg
	NofenceMessage grps;
	proto_InitHeader(&grps);
	gprs.m.gprs_stat_msg.has_xGprsErrorDetails = true;
	gprs.m.gprs_stat_msg.xGprsErrorDetails = ...
	*/
	proto_InitHeader(&seq_1); /* fill up message header. */
	proto_InitHeader(&seq_2); /* fill up message header. */

	seq_1.m.seq_msg.has_usBatteryVoltage = true;
	seq_1.m.seq_msg.usBatteryVoltage = (uint16_t)atomic_get(&cached_batt);
	seq_1.m.seq_msg.has_usChargeMah = true;
	seq_1.m.seq_msg.usChargeMah = (uint16_t)atomic_get(&cached_chrg);
	//seq_1.m.seq_msg.xGprsRssi = cached_rssi; // not implemented
	seq_1.m.seq_msg.has_xHistogramCurrentProfile = true;
	seq_1.m.seq_msg.has_xHistogramZone = true;
	seq_1.m.seq_msg.has_xHistogramAnimalBehave = true;
	memcpy(&seq_1.m.seq_msg.xHistogramAnimalBehave,
	       &histogram.animal_behave, sizeof(histogram.animal_behave));
	memcpy(&seq_1.m.seq_msg.xHistogramCurrentProfile,
	       &histogram.current_profile, sizeof(histogram.current_profile));
	memcpy(&seq_1.m.seq_msg.xHistogramZone, &histogram.in_zone,
	       sizeof(histogram.in_zone));
	seq_1.which_m = (uint16_t)NofenceMessage_seq_msg_tag;

	seq_2.m.seq_msg_2.has_bme280 = true;
	seq_2.m.seq_msg_2.bme280.ulPressure =
		(uint32_t)atomic_get(&cached_press);
	seq_2.m.seq_msg_2.bme280.ulTemperature =
		(uint32_t)atomic_get(&cached_temp);
	seq_2.m.seq_msg_2.bme280.ulHumidity = (uint32_t)atomic_get(&cached_hum);
	seq_2.which_m = (uint16_t)NofenceMessage_seq_msg_2_tag;
	seq_2.m.seq_msg_2.has_xBatteryQc = true;
	seq_2.m.seq_msg_2.xBatteryQc.usVbattMax =
		histogram.qc_battery.usVbattMax;
	seq_2.m.seq_msg_2.xBatteryQc.usVbattMin =
		histogram.qc_battery.usVbattMin;
	seq_2.m.seq_msg_2.xBatteryQc.usTemperature =
		(uint16_t)atomic_get(&cached_temp);

	LOG_DBG("Max_voltage: %u, min voltage: %u, temp: %u",
		seq_2.m.seq_msg_2.xBatteryQc.usVbattMax,
		seq_2.m.seq_msg_2.xBatteryQc.usVbattMin,
		seq_2.m.seq_msg_2.xBatteryQc.usTemperature);

	uint8_t header_size = 2;
	uint8_t encoded_msg_seq_1[NofenceMessage_size + header_size];
	uint8_t encoded_msg_seq_2[NofenceMessage_size + header_size];

	memset(encoded_msg_seq_1, 0, sizeof(encoded_msg_seq_1));
	memset(encoded_msg_seq_2, 0, sizeof(encoded_msg_seq_2));

	size_t encoded_seq_1_size = 0;

	size_t encoded_seq_2_size = 0;

	/* Encode the seq_1 struct created. */
	err = collar_protocol_encode(&seq_1, &encoded_msg_seq_1[2],
				     NofenceMessage_size, &encoded_seq_1_size);
	if (err) {
		LOG_ERR("Error encoding nofence message seq 1 (%i)", err);
		return;
	}

	/* Store the length of the message in the two first bytes */
	encoded_msg_seq_1[0] = (uint8_t)encoded_seq_1_size;
	encoded_msg_seq_1[1] = (uint8_t)(encoded_seq_1_size >> 8);

	/* Encode the seq_2 struct created. */
	err = collar_protocol_encode(&seq_2, &encoded_msg_seq_2[2],
				     NofenceMessage_size, &encoded_seq_2_size);
	if (err) {
		LOG_ERR("Error encoding nofence message seq 2 (%i)", err);
		return;
	}

	/* Store the length of the message in the two first bytes */
	encoded_msg_seq_2[0] = (uint8_t)encoded_seq_2_size;
	encoded_msg_seq_2[1] = (uint8_t)(encoded_seq_2_size >> 8);

	/* Store the encoded message to external flash ring buffer */
	stg_write_log_data(encoded_msg_seq_1, encoded_seq_1_size + header_size);
	stg_write_log_data(encoded_msg_seq_2, encoded_seq_2_size + header_size);
}

int read_log_data_cb(uint8_t *data, size_t len)
{
	LOG_DBG("Send log message fetched from flash");
	/* Fetch the lenght from the two first bytes */
	uint16_t new_len = (uint16_t)((data[1] << 8) + (data[0] & 0x00ff) + 2);
	uint8_t *new_data = k_malloc(new_len);
	memcpy(new_data, &data[0], new_len);
	int err = send_binary_message(new_data, new_len);
	if (err) {
		LOG_ERR("Error sending binary message for log data %i", err);
	}
	k_free(new_data);
	return err;
}

/**
 * @brief Build, send a log request, and reschedule after
 *        "log_period_minutes" minutes.
 */
void log_data_periodic_fn()
{
	/* Reschedule. */
	k_work_reschedule_for_queue(&send_q, &log_work,
				    K_MINUTES(atomic_get(&log_period_minutes)));
	/* Construct log data and write to storage controller. */
	build_log_message();

	/* Read and send out all the log data if any. */
	int err = stg_read_log_data(read_log_data_cb, 0);
	if (err && err != -ENODATA) {
		LOG_ERR("Error reading all sequence messages from storage %i",
			err);
	} else if (err == -ENODATA) {
		LOG_INF("No log data available on flash for sending.");
	}

	/* If all entries has been consumed, empty storage
	 * and we HAVE data on the partition. 
	 */
	if (stg_log_pointing_to_last()) {
		err = stg_clear_partition(STG_PARTITION_LOG);
		if (err) {
			LOG_ERR("Error clearing FCB storage for LOG %i", err);
		}
		LOG_INF("Emptied LOG partition data as we have read everything.");
	}
}

/**
 * @brief Build, send a poll request, and reschedule after
 * "poll_period_minutes".
 */
void modem_poll_work_fn()
{
	k_work_reschedule_for_queue(
		&send_q, &modem_poll_work,
		K_MINUTES(atomic_get(&poll_period_minutes)));
	/* Add logic for the periodic protobuf modem poller. */
	NofenceMessage new_poll_msg;

	if (k_sem_take(&cache_lock_sem, K_SECONDS(1)) == 0) {
		build_poll_request(&new_poll_msg);
		encode_and_send_message(&new_poll_msg);
		k_sem_give(&cache_lock_sem);
	} else {
		LOG_ERR("Cached state semaphore hanged, retrying in 1 second.");
		k_work_reschedule_for_queue(&send_q, &modem_poll_work,
					    K_SECONDS(1));
		return;
	}
}

static int32_t sec_since_gnss_time = 0;

static void zap_message_work_fn()
{
	NofenceMessage msg;
	proto_InitHeader(&msg); /* fill up message header. */
	msg.which_m = (uint16_t)NofenceMessage_client_zap_message_tag;
	msg.m.client_zap_message.has_sFenceDist = false;
	proto_get_last_known_date_pos(&cached_fix,
				      &msg.m.client_zap_message.xDatePos);

	int ret = encode_and_send_message(&msg);
	if (ret) {
		LOG_ERR("Failed to encode zap status msg: %d", ret);
	}
}

static void animal_escaped_work_fn()
{
	NofenceMessage msg;
	proto_InitHeader(&msg); /* fill up message header. */
	msg.which_m = (uint16_t)NofenceMessage_status_msg_tag;
	msg.m.status_msg.has_datePos = true;
	proto_get_last_known_date_pos(&cached_fix, &msg.m.status_msg.datePos);
	msg.m.status_msg.eMode = current_state.collar_mode;
	msg.m.status_msg.eReason = Reason_WARNSTOPREASON_ESCAPED;
	msg.m.status_msg.eCollarStatus = current_state.collar_status;
	msg.m.status_msg.eFenceStatus = current_state.fence_status;
	msg.m.status_msg.usBatteryVoltage = (uint16_t)atomic_get(&cached_batt);
	msg.m.status_msg.has_ucGpsMode = true;
	msg.m.status_msg.ucGpsMode = (uint8_t)cached_gnss_mode;
	int ret = encode_and_send_message(&msg);
	if (ret) {
		LOG_ERR("Failed to encode escaped status msg: %d", ret);
	}
}

static void warning_work_fn()
{
	NofenceMessage msg;
	proto_InitHeader(&msg); /* fill up message header. */
	msg.which_m = (uint16_t)NofenceMessage_client_warning_message_tag;
	msg.m.client_warning_message.has_sFenceDist = false;
	proto_get_last_known_date_pos(&cached_fix,
				      &msg.m.client_zap_message.xDatePos);

	int ret = encode_and_send_message(&msg);
	if (ret) {
		LOG_ERR("Failed to encode warning msg: %d", ret);
	}
}

static void warning_start_work_fn()
{
	NofenceMessage msg;
	proto_InitHeader(&msg); /* fill up message header. */
	msg.which_m =
		(uint16_t)NofenceMessage_client_correction_start_message_tag;
	msg.m.client_correction_start_message.has_sFenceDist = false;
	proto_get_last_known_date_pos(
		&cached_fix, &msg.m.client_correction_start_message.xDatePos);

	int ret = encode_and_send_message(&msg);
	if (ret) {
		LOG_ERR("Failed to encode warning start msg: %d", ret);
	}
}

static void warning_end_work_fn()
{
	NofenceMessage msg;
	proto_InitHeader(&msg); /* fill up message header. */
	msg.which_m =
		(uint16_t)NofenceMessage_client_correction_end_message_tag;
	msg.m.client_correction_end_message.has_sFenceDist = false;
	proto_get_last_known_date_pos(
		&cached_fix, &msg.m.client_correction_end_message.xDatePos);

	int ret = encode_and_send_message(&msg);
	if (ret) {
		LOG_ERR("Failed to encode warning start msg: %d", ret);
	}
}
/**
 * @brief Work function to periodic request sensor data etc.
 */
void data_request_work_fn()
{
	LOG_INF("Periodic request data");
	k_work_reschedule_for_queue(&send_q, &data_request_work, K_MINUTES(1));

	/* Request of battery voltage */
	struct request_pwr_battery_event *ev_batt =
		new_request_pwr_battery_event();
	EVENT_SUBMIT(ev_batt);

	/* Request of charging current */
	struct request_pwr_charging_event *ev_charge =
		new_request_pwr_charging_event();
	EVENT_SUBMIT(ev_charge);

	/* Request of temp, press, humidity */
	struct request_env_sensor_event *ev_env =
		new_request_env_sensor_event();
	EVENT_SUBMIT(ev_env);
}
/**
 * @brief Main event handler function. 
 * 
 * @param[in] eh Event_header for the if-chain to 
 *               use to recognize which event triggered.
 * 
 * @return True or false based on if we want to consume the event or not.
 */
static bool event_handler(const struct event_header *eh)
{
	if (is_gnss_data(eh)) {
		/* Check when we received server time last. */
		int32_t server_time =
			(int32_t)atomic_get(&server_timestamp_sec);
		int32_t delta_server_time =
			(int32_t)(k_uptime_get_32() / 1000) - server_time;

		/* If we already have server time, just return. */
		if (delta_server_time <= TWO_DAYS_SEC && server_time != 0) {
			return false;
		}

		/* Check if we can use GNSS data based on the pvt_valid bits
		 * and timestamp since correction. Doesn't need to be
		 * atomic, since event handler thead is the only one
		 * accessing it.
		 */
		struct gnss_data *ev = cast_gnss_data(eh);

		if (ev->gnss_data.fix_ok && ev->gnss_data.has_lastfix) {
			if (k_sem_take(&cache_lock_sem, K_MSEC(200)) == 0) {
				cached_fix = ev->gnss_data.lastfix;
				k_sem_give(&cache_lock_sem);
			}
		}
		if (!(ev->gnss_data.latest.pvt_valid & (1 << 0)) ||
		    !(ev->gnss_data.latest.pvt_valid & (1 << 1))) {
			return false;
		}

		if ((int32_t)(k_uptime_get_32() / 1000) - sec_since_gnss_time <
		    SEC_HOUR) {
			return false;
		}

		/** @todo Check if uint32_t to time_t typecast works. */
		time_t gm_time = (time_t)ev->gnss_data.lastfix.unix_timestamp;
		struct tm *tm_time = gmtime(&gm_time);
		/* Update date_time library which storage uses for ANO data. */
		if (!date_time_set(tm_time)) {
			LOG_ERR("Could not set date time from GNSS data");
			return false;
		} else {
			LOG_INF("Now using GNSS unix timestamp instead: %s",
				asctime(tm_time));
		}

		sec_since_gnss_time = (int32_t)(k_uptime_get_32() / 1000);

		cached_gnss_mode = (gnss_mode_t)ev->gnss_data.lastfix.mode;
		return false;
	}

	if (is_ble_ctrl_event(eh)) {
		struct ble_ctrl_event *ev = cast_ble_ctrl_event(eh);
		while (k_msgq_put(&ble_ctrl_msgq, ev, K_NO_WAIT) != 0) {
			/* Message queue is full: purge old data & try again */
			k_msgq_purge(&ble_ctrl_msgq);
		}
		return false;
	}
	if (is_ble_cmd_event(eh)) {
		struct ble_cmd_event *ev = cast_ble_cmd_event(eh);
		while (k_msgq_put(&ble_cmd_msgq, ev, K_NO_WAIT) != 0) {
			k_msgq_purge(&ble_cmd_msgq);
		}
		return false;
	}
	if (is_ble_data_event(eh)) {
		struct ble_data_event *ev = cast_ble_data_event(eh);
		while (k_msgq_put(&ble_data_msgq, ev, K_NO_WAIT) != 0) {
			k_msgq_purge(&ble_data_msgq);
		}
		return false;
	}
	if (is_cellular_proto_in_event(eh)) {
		struct cellular_proto_in_event *ev =
			cast_cellular_proto_in_event(eh);
		while (k_msgq_put(&lte_proto_msgq, ev, K_NO_WAIT) != 0) {
			k_msgq_purge(&lte_proto_msgq);
		}
		return false;
	}
	if (is_update_collar_mode(eh)) {
		struct update_collar_mode *ev = cast_update_collar_mode(eh);
		current_state.collar_mode = ev->collar_mode;
		return false;
	}
	if (is_update_collar_status(eh)) {
		struct update_collar_status *ev = cast_update_collar_status(eh);
		current_state.collar_status = ev->collar_status;
		return false;
	}
	if (is_update_fence_status(eh)) {
		struct update_fence_status *ev = cast_update_fence_status(eh);
		current_state.fence_status = ev->fence_status;
		return false;
	}
	if (is_update_fence_version(eh)) {
		struct update_fence_version *ev = cast_update_fence_version(eh);
		current_state.fence_version = ev->fence_version;

		/* Notify server as soon as the new fence is activated. */
		int err = k_work_reschedule_for_queue(&send_q, &modem_poll_work,
						      K_NO_WAIT);
		if (err < 0) {
			LOG_ERR("Error starting modem poll worker: %d", err);
		}
		return false;
	}
	if (is_update_flash_erase(eh)) {
		current_state.flash_erase_count++;
		return false;
	}
	if (is_update_zap_count(eh)) {
		struct update_zap_count *ev = cast_update_zap_count(eh);
		current_state.zap_count = ev->count;

		int err = k_work_reschedule_for_queue(
			&send_q, &process_zap_work, K_NO_WAIT);
		if (err < 0) {
			LOG_ERR("Error reschedule zap work: %d", err);
		}
		return false;
	}
	if (is_cellular_ack_event(eh)) {
		k_sem_give(&send_out_ack);
		return false;
	}
	if (is_gnss_data(eh)) {
		struct gnss_data *ev = cast_gnss_data(eh);
		if (ev->gnss_data.fix_ok && ev->gnss_data.has_lastfix) {
			if (k_sem_take(&cache_lock_sem, K_MSEC(500)) == 0) {
				cached_fix = ev->gnss_data.lastfix;
				k_sem_give(&cache_lock_sem);
			}
		}
		return false;
	}

	if (is_animal_escape_event(eh)) {
		int err = k_work_reschedule_for_queue(
			&send_q, &process_escape_work, K_NO_WAIT);
		if (err < 0) {
			LOG_ERR("Error reschedule escape work %d", err);
		}
		return false;
	}

	if (is_connection_state_event(eh)) {
		struct connection_state_event *ev =
			cast_connection_state_event(eh);
		if (ev->state) {
			k_sem_give(&connection_ready);
		} else {
			/*TODO: take some action while waiting for cellular
			 * controller to recover the connection.*/
		}
		return false;
	}

	if (is_animal_warning_event(eh)) {
		int err = k_work_reschedule_for_queue(
			&send_q, &process_warning_work, K_NO_WAIT);
		if (err < 0) {
			LOG_ERR("Error reschedule warning work: %d", err);
		}

		return false;
	}
	if (is_pwr_status_event(eh)) {
		struct pwr_status_event *ev = cast_pwr_status_event(eh);
		/* Update shaddow register */
		if (ev->pwr_state == PWR_BATTERY) {
			/* We want battery voltage in deci volt */
			atomic_set(&cached_batt,
				   (uint16_t)(ev->battery_mv / 10));
			LOG_DBG("Battery event: %u mV", ev->battery_mv);
		} else if (ev->pwr_state == PWR_CHARGING) {
			LOG_DBG("Charge event: %u mA", ev->charging_ma);
			atomic_set(&cached_chrg, ev->charging_ma);
		}
		return false;
	}
	if (is_send_poll_request_now(eh)) {
		LOG_DBG("Received a nudge on listening socket!");
		int err = k_work_reschedule_for_queue(&send_q, &modem_poll_work,
						      K_NO_WAIT);
		if (err < 0) {
			char *e_msg =
				"Error starting modem poll worker in response to nudge on listening socket.";
			LOG_ERR("%s (%d)", log_strdup(e_msg), err);
			nf_app_error(ERR_MESSAGING, err, e_msg, strlen(e_msg));
		}
		return false;
	}
	if (is_env_sensor_event(eh)) {
		struct env_sensor_event *ev = cast_env_sensor_event(eh);
		LOG_DBG("Event Temp: %.2f, humid %.2f, press %.2f", ev->temp,
			ev->humidity, ev->press);
		/* Update shaddow register */
		atomic_set(&cached_press, (uint32_t)ev->press);
		atomic_set(&cached_hum, (uint32_t)ev->humidity);
		atomic_set(&cached_temp, (uint32_t)ev->temp);
		return false;
	}
	if (is_warn_correction_start_event(eh)) {
		int err = k_work_reschedule_for_queue(
			&send_q, &process_warning_correction_start_work,
			K_NO_WAIT);
		if (err < 0) {
			LOG_ERR("Error reschedule warning correction start work: %d",
				err);
		}
		return false;
	}
	if (is_warn_correction_end_event(eh)) {
		int err = k_work_reschedule_for_queue(
			&send_q, &process_warning_correction_end_work,
			K_NO_WAIT);
		if (err < 0) {
			LOG_ERR("Error reschedule warning correction start work: %d",
				err);
		}
		return false;
	}

	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}

bool validate_pasture()
{
	uint16_t crc = 0xFFFF;

	uint16_t pasture_value_16;
	uint32_t pasture_value_32;

	pasture_value_32 = pasture_temp.m.ul_fence_def_version;
	crc = nf_crc16_uint32(pasture_value_32, &crc);

	pasture_value_32 = pasture_temp.m.l_origin_lon;
	crc = nf_crc16_uint32(pasture_value_32, &crc);

	pasture_value_32 = pasture_temp.m.l_origin_lat;
	crc = nf_crc16_uint32(pasture_value_32, &crc);

	pasture_value_16 = pasture_temp.m.us_k_lon;
	crc = nf_crc16_uint16(pasture_value_16, &crc);

	pasture_value_16 = pasture_temp.m.us_k_lat;
	crc = nf_crc16_uint16(pasture_value_16, &crc);

	for (uint8_t i = 0; i < pasture_temp.m.ul_total_fences; i++) {
		fence_t *target_fence = &pasture_temp.fences[i];
		for (uint8_t j = 0; j < target_fence->m.n_points; j++) {
			pasture_value_16 = target_fence->coordinates[j].s_x_dm;
			crc = nf_crc16_uint16(pasture_value_16, &crc);

			pasture_value_16 = target_fence->coordinates[j].s_y_dm;
			crc = nf_crc16_uint16(pasture_value_16, &crc);
		}
	}

	return crc == pasture_temp.m.us_pasture_crc;
}

EVENT_LISTENER(MODULE, event_handler);

/* Bluetooth events. */
EVENT_SUBSCRIBE(MODULE, ble_ctrl_event);
EVENT_SUBSCRIBE(MODULE, ble_cmd_event);
EVENT_SUBSCRIBE(MODULE, ble_data_event);

EVENT_SUBSCRIBE(MODULE, lte_proto_event);
EVENT_SUBSCRIBE(MODULE, cellular_ack_event);
EVENT_SUBSCRIBE(MODULE, cellular_proto_in_event);
EVENT_SUBSCRIBE(MODULE, update_collar_mode);
EVENT_SUBSCRIBE(MODULE, update_collar_status);
EVENT_SUBSCRIBE(MODULE, update_fence_status);
EVENT_SUBSCRIBE(MODULE, update_fence_version);
EVENT_SUBSCRIBE(MODULE, update_flash_erase);
EVENT_SUBSCRIBE(MODULE, update_zap_count);
EVENT_SUBSCRIBE(MODULE, animal_warning_event);
EVENT_SUBSCRIBE(MODULE, animal_escape_event);
EVENT_SUBSCRIBE(MODULE, connection_state_event);
EVENT_SUBSCRIBE(MODULE, pwr_status_event);
EVENT_SUBSCRIBE(MODULE, env_sensor_event);
/** @todo add battery, histogram, gnss and modem event */
EVENT_SUBSCRIBE(MODULE, gnss_data);
EVENT_SUBSCRIBE(MODULE, send_poll_request_now);
EVENT_SUBSCRIBE(MODULE, warn_correction_start_event);
EVENT_SUBSCRIBE(MODULE, warn_correction_end_event);

static inline void process_ble_ctrl_event(void)
{
	struct ble_ctrl_event ev;

	int err = k_msgq_get(&ble_ctrl_msgq, &ev, K_NO_WAIT);
	if (err) {
		char *e_msg = "Error getting ble_ctrl_event";
		LOG_ERR("%s (%d)", log_strdup(e_msg), err);
		nf_app_error(ERR_MESSAGING, err, e_msg, strlen(e_msg));
		return;
	}
}

static inline void process_ble_data_event(void)
{
	struct ble_data_event ev;

	int err = k_msgq_get(&ble_data_msgq, &ev, K_NO_WAIT);
	if (err) {
		char *e_msg = "Error getting ble_data_event";
		LOG_ERR("%s (%d)", log_strdup(e_msg), err);
		nf_app_error(ERR_MESSAGING, err, e_msg, strlen(e_msg));
		return;
	}
}

static inline void process_ble_cmd_event(void)
{
	struct ble_cmd_event ev;

	int err = k_msgq_get(&ble_cmd_msgq, &ev, K_NO_WAIT);
	if (err) {
		char *e_msg = "Error getting ble_cmd_event";
		LOG_ERR("%s (%d)", log_strdup(e_msg), err);
		nf_app_error(ERR_MESSAGING, err, e_msg, strlen(e_msg));
		return;
	}

	enum command_char ble_command = ev.cmd;

	switch (ble_command) {
	case CMD_TURN_OFF_FENCE: {
		/* Wait for final AMC integration. Should simply issue an event. */
		break;
	}
	case CMD_REBOOT_AVR_MCU: {
		struct pwr_reboot_event *r_ev = new_pwr_reboot_event();
		EVENT_SUBMIT(r_ev);
		break;
	}
	case CMD_PLAY_SOUND: {
		struct sound_event *s_ev = new_sound_event();
		s_ev->type = SND_FIND_ME;
		EVENT_SUBMIT(s_ev);
		break;
	}
	case CMD_DOWNLOAD_FENCE: {
		/* Downloads fence from bluetooth? */
		break;
	}
	case CMD_FW_INSTALL: {
		/* Deprecated as we have ble_dfu.c instead? */
		break;
	}
	default: {
		break;
	}
	}
}

static void process_lte_proto_event(void)
{
	struct cellular_proto_in_event ev;

	int err = k_msgq_get(&lte_proto_msgq, &ev, K_NO_WAIT);
	if (err) {
		char *e_msg = "Error getting lte_proto_event";
		LOG_ERR("%s (%d)", log_strdup(e_msg), err);
		nf_app_error(ERR_MESSAGING, err, e_msg, strlen(e_msg));
		return;
	}

	NofenceMessage proto;
	err = collar_protocol_decode(ev.buf + 2, ev.len - 2, &proto);

	/** @note Bytes debug?
	 *
	 * LOG_INF("Number of received bytes = %d", ev.len);
 	 * 
	 * char *buf = ev.buf;
	 * for (int i = 0; i < ev.len; i++) {
	 * 	printk("\\x%02x", *buf);
	 * 	buf++;
	 * }
	 */

	if (err) {
		char *e_msg = "Error decoding protobuf message";
		LOG_ERR("%s (%d)", log_strdup(e_msg), err);
		nf_app_error(ERR_MESSAGING, err, e_msg, strlen(e_msg));
		return;
	}
	struct messaging_ack_event *ack = new_messaging_ack_event();
	EVENT_SUBMIT(ack);
	/* process poll response */
	if (proto.which_m == NofenceMessage_poll_message_resp_tag) {
		LOG_INF("Process poll reponse");
		process_poll_response(&proto);
		return;
	} else if (proto.which_m == NofenceMessage_fence_definition_resp_tag) {
		uint8_t new_fframe =
			process_fence_msg(&proto.m.fence_definition_resp);
		fence_download(new_fframe);
		return;
	} else if (proto.which_m == NofenceMessage_ubx_ano_reply_tag) {
		uint16_t new_ano_frame =
			process_ano_msg(&proto.m.ubx_ano_reply);
		ano_download(proto.m.ubx_ano_reply.usAnoId, new_ano_frame);
		return;
	} else {
		return;
	}
}
void messaging_thread_fn()
{
	while (true) {
		int rc = k_poll(msgq_events, NUM_MSGQ_EVENTS, K_FOREVER);
		if (rc == 0) {
			while (k_msgq_num_used_get(&ble_ctrl_msgq) > 0) {
				process_ble_ctrl_event();
			}
			while (k_msgq_num_used_get(&ble_cmd_msgq) > 0) {
				process_ble_cmd_event();
			}
			while (k_msgq_num_used_get(&ble_data_msgq) > 0) {
				process_ble_data_event();
			}
			while (k_msgq_num_used_get(&lte_proto_msgq) > 0) {
				process_lte_proto_event();
			}
		}
		/* Set all the events to not ready again. */
		for (int i = 0; i < NUM_MSGQ_EVENTS; i++) {
			msgq_events[i].state = K_POLL_STATE_NOT_READY;
		}
	}
}

int messaging_module_init(void)
{
	LOG_INF("Initializing messaging module.");
	int err = eep_uint32_read(EEP_UID, &serial_id);
	if (err != 0) {
		char *e_msg = "Failed to read serial number from eeprom!";
		LOG_ERR("%s (%d)", log_strdup(e_msg), err);
		nf_app_error(ERR_MESSAGING, err, e_msg, strlen(e_msg));
		return err;
	}

	k_work_queue_init(&send_q);
	k_work_queue_start(&send_q, messaging_send_thread,
			   K_THREAD_STACK_SIZEOF(messaging_send_thread),
			   CONFIG_MESSAGING_SEND_THREAD_PRIORITY, NULL);

	k_work_init_delayable(&modem_poll_work, modem_poll_work_fn);
	k_work_init_delayable(&log_work, log_data_periodic_fn);
	k_work_init_delayable(&data_request_work, data_request_work_fn);
	k_work_init_delayable(&process_escape_work, animal_escaped_work_fn);
	k_work_init_delayable(&process_zap_work, zap_message_work_fn);
	k_work_init_delayable(&process_warning_work, warning_work_fn);
	k_work_init_delayable(&process_warning_correction_start_work,
			      warning_start_work_fn);
	k_work_init_delayable(&process_warning_correction_end_work,
			      warning_end_work_fn);

	memset(&pasture_temp, 0, sizeof(pasture_t));
	cached_fences_counter = 0;
	pasture_temp.m.us_pasture_crc = EMPTY_FENCE_CRC;

	/** @todo Should add semaphore and only start these queues when
	 *  we get connection to network with modem.
	 */
	err = k_work_schedule_for_queue(&send_q, &data_request_work, K_NO_WAIT);
	if (err < 0) {
		return err;
	}
	err = k_work_schedule_for_queue(&send_q, &modem_poll_work,
					K_SECONDS(2));
	if (err < 0) {
		return err;
	}
	err = k_work_schedule_for_queue(&send_q, &log_work, K_SECONDS(2));
	if (err < 0) {
		return err;
	}

	return 0;
}

void build_poll_request(NofenceMessage *poll_req)
{
	proto_InitHeader(poll_req); /* fill up message header. */
	poll_req->which_m = NofenceMessage_poll_message_req_tag;
	proto_get_last_known_date_pos(&cached_fix,
				      &poll_req->m.poll_message_req.datePos);
	poll_req->m.poll_message_req.has_datePos =
		proto_has_last_known_date_pos(&cached_fix);
	poll_req->m.poll_message_req.eMode = current_state.collar_mode;
	poll_req->m.poll_message_req.usZapCount = current_state.zap_count;
	poll_req->m.poll_message_req.eCollarStatus =
		current_state.collar_status;
	poll_req->m.poll_message_req.eFenceStatus = current_state.fence_status;
	poll_req->m.poll_message_req.ulFenceDefVersion =
		current_state.fence_version;
	poll_req->m.poll_message_req.usBatteryVoltage =
		(uint16_t)atomic_get(&cached_batt);
	poll_req->m.poll_message_req.has_ucMCUSR = 0;
	poll_req->m.poll_message_req.ucMCUSR = 0;

	/* Fw info. */
	poll_req->m.poll_message_req.has_versionInfoHW = true;
	poll_req->m.poll_message_req.versionInfoHW.ucPCB_RF_Version = 1;
	poll_req->m.poll_message_req.versionInfoHW.ucPCB_HV_Version = 1;
	poll_req->m.poll_message_req.versionInfoHW.usPCB_Product_Type = 1;
	/* TODO: get gsm info from modem driver */
	//	const _GSM_INFO *p_gsm_info = bgs_get_gsm_info();
	//	poll_req.m.poll_message_req.xGsmInfo = *p_gsm_info;
	//	poll_req->m.poll_message_req.has_xGsmInfo = false;

	if (current_state.flash_erase_count) {
		// m_flash_erase_count is reset when we receive a poll reply
		poll_req->m.poll_message_req.has_usFlashEraseCount = true;
		poll_req->m.poll_message_req.usFlashEraseCount =
			current_state.flash_erase_count;
	}
	if (m_confirm_acc_limits) {
		/** @warning Assumes all the activity values are given with the
		 *  m_confirm_acc_limits flag set in poll response from server.
		 */
		poll_req->m.poll_message_req.has_usAccSigmaSleepLimit = true;
		poll_req->m.poll_message_req.has_usAccSigmaNoActivityLimit =
			true;
		poll_req->m.poll_message_req.has_usOffAnimalTimeLimitSec = true;

		int err = eep_uint16_read(
			EEP_ACC_SIGMA_SLEEP_LIMIT,
			&poll_req->m.poll_message_req.usAccSigmaSleepLimit);
		if (err) {
			char *e_msg =
				"Failed to read EEP_ACC_SIGMA_SLEEP_LIMIT";
			LOG_ERR("%s (%d)", log_strdup(e_msg), err);
			nf_app_error(ERR_MESSAGING, err, e_msg, strlen(e_msg));
		}

		err = eep_uint16_read(
			EEP_ACC_SIGMA_NOACTIVITY_LIMIT,
			&poll_req->m.poll_message_req.usAccSigmaNoActivityLimit);
		if (err) {
			char *e_msg =
				"Failed to read EEP_ACC_SIGMA_NOACTIVITY_LIMIT";
			LOG_ERR("%s (%d)", log_strdup(e_msg), err);
			nf_app_error(ERR_MESSAGING, err, e_msg, strlen(e_msg));
		}

		err = eep_uint16_read(
			EEP_OFF_ANIMAL_TIME_LIMIT_SEC,
			&poll_req->m.poll_message_req.usOffAnimalTimeLimitSec);
		if (err) {
			char *e_msg =
				"Failed to read EEP_OFF_ANIMAL_TIME_LIMIT_SEC";
			LOG_ERR("%s (%d)", log_strdup(e_msg), err);
			nf_app_error(ERR_MESSAGING, err, e_msg, strlen(e_msg));
		}
	}
	if (m_confirm_ble_key) {
		poll_req->m.poll_message_req.has_rgubcBleKey = true;
		poll_req->m.poll_message_req.rgubcBleKey.size =
			EEP_BLE_SEC_KEY_LEN;
		int err = eep_read_ble_sec_key(
			poll_req->m.poll_message_req.rgubcBleKey.bytes,
			EEP_BLE_SEC_KEY_LEN);
		if (err) {
			char *e_msg = "Failed to read ble_sec_key";
			LOG_ERR("%s (%d)", log_strdup(e_msg), err);
			nf_app_error(ERR_MESSAGING, err, e_msg, strlen(e_msg));
		}
	}
	poll_req->m.poll_message_req.usGnssOnFixAgeSec = 123;
	poll_req->m.poll_message_req.usGnssTTFFSec = 12;

	if (m_transfer_boot_params) {
		/** @todo?????	
		  * poll_req.m.poll_message_req.has_versionInfo = true;
		  * uint16_t xbootVersion;
		  * if (xboot_get_version(&xbootVersion) == XB_SUCCESS) {
		  * 	poll_req.m.poll_message_req.versionInfo
		  * 		.usATmegaBootloaderVersion =
		  * 		xbootVersion;
		  * 	poll_req.m.poll_message_req.versionInfo
		  * 		.has_usATmegaBootloaderVersion = true;
		  * }
		  * poll_req.m.poll_message_req.has_versionInfoHW = true;
		  * poll_req.m.poll_message_req.versionInfoHW.ucPCB_RF_Version =
		  * 	EEPROM_GetHwVersion();
		  * poll_req.m.poll_message_req.versionInfoHW.usPCB_Product_Type =
		  * 	(uint8_t)EEPROM_GetProductType();
		  * poll_req.m.poll_message_req.has_xSimCardId = true;
		  * memcpy(poll_req.m.poll_message_req.xSimCardId, BGS_SCID(),
		  *        sizeof(poll_req.m.poll_message_req.xSimCardId));
		  * poll_req.m.poll_message_req.versionInfo.has_ulATmegaVersion =
		  * 	true;
		  * poll_req.m.poll_message_req.versionInfo.ulATmegaVersion =
		  *			NF_X25_VERSION_NUMBER;
		  */
	}
}

int8_t request_fframe(uint32_t version, uint8_t frame)
{
	NofenceMessage fence_req;
	proto_InitHeader(&fence_req); /* fill up message header. */
	fence_req.which_m = NofenceMessage_fence_definition_req_tag;
	fence_req.m.fence_definition_req.ulFenceDefVersion = version;
	fence_req.m.fence_definition_req.ucFrameNumber = frame;
	int ret = encode_and_send_message(&fence_req);
	if (ret) {
		char *e_msg = "Failed to send request for fence frame";
		LOG_ERR("%s %d (%d)", log_strdup(e_msg), frame, ret);
		nf_app_error(ERR_MESSAGING, ret, e_msg, strlen(e_msg));
		return -1;
	}
	return 0;
}

void fence_download(uint8_t new_fframe)
{
	if (new_fframe == 0 && first_frame) {
		first_frame = false;
	} else if (new_fframe == 0 && !first_frame) { //something went bad
		expected_fframe = 0;
		new_fence_in_progress = 0;
		return;
	}
	if (new_fframe >= 0) {
		if (new_fframe == DOWNLOAD_COMPLETE) {
			/* Notify AMC that a new fence is available. */
			struct new_fence_available *fence_ready =
				new_new_fence_available();
			EVENT_SUBMIT(fence_ready);

			LOG_INF("Fence ver %d download complete and notified AMC.",
				new_fence_in_progress);

			expected_fframe = 0;
			return;
		}
		if (new_fframe == expected_fframe) {
			expected_fframe++;
			/* TODO: handle failure to send request!*/
			request_fframe(new_fence_in_progress, expected_fframe);
			LOG_INF("Requesting frame %d of new fence: %d",
				expected_fframe, new_fence_in_progress);
		}
	}
}

int8_t request_ano_frame(uint16_t ano_id, uint16_t ano_start)
{
	NofenceMessage ano_req;
	proto_InitHeader(&ano_req); /* fill up message header. */
	ano_req.which_m = NofenceMessage_ubx_ano_req_tag;
	ano_req.m.ubx_ano_req.usAnoId = ano_id;
	ano_req.m.ubx_ano_req.usStartAno = ano_start;
	int ret = encode_and_send_message(&ano_req);
	if (ret) {
		char *e_msg = "Failed to send request for ano";
		LOG_ERR("%s %d (%d)", log_strdup(e_msg), ano_start, ret);
		nf_app_error(ERR_MESSAGING, ret, e_msg, strlen(e_msg));
		return -1;
	}
	return 0;
}

void ano_download(uint16_t ano_id, uint16_t new_ano_frame)
{
	if (first_ano_frame) {
		new_ano_in_progress = ano_id;
		expected_ano_frame = 0;
		first_ano_frame = false;
	} else if (new_ano_frame == 0 && !first_ano_frame) {
		expected_ano_frame = 0;
		new_ano_in_progress = 0;
		return;
	}
	if (new_ano_frame >= 0) {
		if (new_ano_frame == DOWNLOAD_COMPLETE) {
			LOG_INF("ANO %d download complete.",
				new_ano_in_progress);
			return;
		}

		expected_ano_frame += new_ano_frame;
		/* TODO: handle failure to send request!*/
		int ret = request_ano_frame(ano_id, expected_ano_frame);

		LOG_INF("Requesting frame %d of new ano: %d.",
			expected_ano_frame, new_ano_in_progress);

		if (ret != 0) {
			/* TODO: reset ANO state and retry later.*/
			expected_ano_frame = 0;
			new_ano_in_progress = 0;
		}
		return;
	}
}

void proto_InitHeader(NofenceMessage *msg)
{
	memset(msg, 0, sizeof(NofenceMessage));
	msg->header.ulId = serial_id;
	msg->header.ulVersion = NF_X25_VERSION_NUMBER;
	msg->header.has_ulVersion = true;
	if (use_server_time) {
		msg->header.ulUnixTimestamp = time_from_server;
	} else {
		msg->header.ulUnixTimestamp = cached_fix.unix_timestamp;
	}
}

/** @brief Sends a binary encoded message to the server through cellular
 *         controller.
 * @param data binary message. @note Assumes 2 first bytes are empty.
 * @param len length of the binary data including the 2 start bytes.
 * 
 * @return 0 on success, otherwise negative errno.
 */
int send_binary_message(uint8_t *data, size_t len)
{
	struct check_connection *ev = new_check_connection();
	EVENT_SUBMIT(ev);
	int ret = k_sem_take(&connection_ready, K_MINUTES(2));
	if (ret != 0) {
		char *e_msg = "Connection not ready, can't send message now!";
		LOG_ERR("%s (%d)", log_strdup(e_msg), ret);
		nf_app_error(ERR_MESSAGING, ret, e_msg, strlen(e_msg));
		return -ETIMEDOUT;
	}
	/* We can only send 1 message at a time, use mutex. */
	if (k_mutex_lock(&send_binary_mutex,
			 K_SECONDS(CONFIG_CC_ACK_TIMEOUT_SEC * 2)) == 0) {
		uint16_t byteswap_size = BYTESWAP16(len - 2);
		memcpy(&data[0], &byteswap_size, 2);

		struct messaging_proto_out_event *msg2send =
			new_messaging_proto_out_event();
		msg2send->buf = data;
		msg2send->len = len;
		EVENT_SUBMIT(msg2send);

		int err = k_sem_take(&send_out_ack,
				     K_SECONDS(CONFIG_CC_ACK_TIMEOUT_SEC));
		if (err) {
			char *e_msg = "Timed out waiting for cellular ack";
			nf_app_error(ERR_MESSAGING, -ETIMEDOUT, e_msg,
				     strlen(e_msg));
			k_mutex_unlock(&send_binary_mutex);
			return -ETIMEDOUT;
		}
		k_mutex_unlock(&send_binary_mutex);
		return 0;
	} else {
		return -ETIMEDOUT;
	}
	return 0;
}

int encode_and_send_message(NofenceMessage *msg_proto)
{
	uint8_t encoded_msg[NofenceMessage_size + 2];
	memset(encoded_msg, 0, sizeof(encoded_msg));
	size_t encoded_size = 0;

	LOG_INF("Start message encoding, size: %d, version: %u",
		sizeof(msg_proto), msg_proto->header.ulVersion);
	int ret = collar_protocol_encode(msg_proto, &encoded_msg[2],
					 NofenceMessage_size, &encoded_size);
	if (ret) {
		char *e_msg = "Error encoding nofence message";
		LOG_ERR("%s (%d)", log_strdup(e_msg), ret);
		nf_app_error(ERR_MESSAGING, ret, e_msg, strlen(e_msg));
		return ret;
	}
	return send_binary_message(encoded_msg, encoded_size + 2);
}

void process_poll_response(NofenceMessage *proto)
{
	PollMessageResponse *pResp = &proto->m.poll_message_resp;
	if (pResp->has_xServerIp && strlen(pResp->xServerIp) > 0) {
		struct messaging_host_address_event *host_add_event =
			new_messaging_host_address_event();
		strncpy(host_add_event->address, pResp->xServerIp,
			sizeof(pResp->xServerIp));
		EVENT_SUBMIT(host_add_event); /*cellular controller writes it
 * to eeprom if it is different from the previously stored address.*/
	}
	if (pResp->has_bEraseFlash && pResp->bEraseFlash) {
		struct request_flash_erase_event *flash_erase_event =
			new_request_flash_erase_event();
		flash_erase_event->magic = STORAGE_ERASE_MAGIC;
		EVENT_SUBMIT(flash_erase_event);
	}
	// If we are asked to, reboot
	if (pResp->has_bReboot && pResp->bReboot) {
		struct pwr_reboot_event *r_ev = new_pwr_reboot_event();
		EVENT_SUBMIT(r_ev);
	}
	/** TODO: set activation mode to (pResp->eActivationMode); 
	  * Not used in legacy code? 
	  */

	if (pResp->has_bUseUbloxAno) {
		/* TODO: publish enable ANO event to GPS controller */
	}
	if (pResp->has_bUseServerTime && pResp->bUseServerTime) {
		LOG_INF("Server time will be used.");
		time_from_server = proto->header.ulUnixTimestamp;
		use_server_time = true;
		time_t gm_time = (time_t)proto->header.ulUnixTimestamp;
		struct tm *tm_time = gmtime(&gm_time);
		/* Updates date_time library which storage uses for ANO data. */
		int err = date_time_set(tm_time);
		if (err) {
			char *e_msg = "Error updating time from server";
			LOG_ERR("%s (%d)", log_strdup(e_msg), err);
			nf_app_error(ERR_MESSAGING, err, e_msg, strlen(e_msg));
		} else {
			/** @note This prints UTC. */
			LOG_INF("Set timestamp to date_time library from modem: %s",
				asctime(tm_time));
			atomic_set(&server_timestamp_sec,
				   (atomic_val_t)(int32_t)(k_uptime_get_32() /
							   1000));
		}
	}
	if (pResp->has_usPollConnectIntervalSec) {
		atomic_set(&poll_period_minutes,
			   pResp->usPollConnectIntervalSec / 60);

		k_work_reschedule_for_queue(
			&send_q, &modem_poll_work,
			K_MINUTES(atomic_get(&poll_period_minutes)));
		LOG_INF("Poll period of %d minutes will be used.",
			atomic_get(&poll_period_minutes));
	}
	m_confirm_acc_limits = false;
	if (pResp->has_usAccSigmaSleepLimit) {
		m_confirm_acc_limits = true;
		int err = eep_uint16_write(EEP_ACC_SIGMA_SLEEP_LIMIT,
					   pResp->usAccSigmaSleepLimit);
		if (err) {
			char *e_msg = "Error updating sleep sigma to eeprom";
			LOG_ERR("%s (%d)", log_strdup(e_msg), err);
			nf_app_error(ERR_MESSAGING, err, e_msg, strlen(e_msg));
		}

		struct acc_sigma_event *sigma_ev = new_acc_sigma_event();
		sigma_ev->type = SLEEP_SIGMA;
		sigma_ev->param.sleep_sigma_value = pResp->usAccSigmaSleepLimit;
		EVENT_SUBMIT(sigma_ev);
	}
	if (pResp->has_usAccSigmaNoActivityLimit) {
		m_confirm_acc_limits = true;
		int err = eep_uint16_write(EEP_ACC_SIGMA_NOACTIVITY_LIMIT,
					   pResp->usAccSigmaNoActivityLimit);
		if (err) {
			char *e_msg =
				"Error updating no activity sigma to eeprom";
			LOG_ERR("%s (%d)", log_strdup(e_msg), err);
			nf_app_error(ERR_MESSAGING, err, e_msg, strlen(e_msg));
		}

		struct acc_sigma_event *sigma_ev = new_acc_sigma_event();
		sigma_ev->type = NO_ACTIVITY_SIGMA;
		sigma_ev->param.no_activity_sigma =
			pResp->usAccSigmaNoActivityLimit;
		EVENT_SUBMIT(sigma_ev);
	}
	if (pResp->has_usOffAnimalTimeLimitSec) {
		m_confirm_acc_limits = true;
		int err = eep_uint16_write(EEP_OFF_ANIMAL_TIME_LIMIT_SEC,
					   pResp->usOffAnimalTimeLimitSec);
		if (err) {
			char *e_msg =
				"Error updating off animal sigma to eeprom";
			LOG_ERR("%s (%d)", log_strdup(e_msg), err);
			nf_app_error(ERR_MESSAGING, err, e_msg, strlen(e_msg));
		}

		struct acc_sigma_event *sigma_ev = new_acc_sigma_event();
		sigma_ev->type = OFF_ANIMAL_SIGMA;
		sigma_ev->param.off_animal_value =
			pResp->usOffAnimalTimeLimitSec;
		EVENT_SUBMIT(sigma_ev);
	}

	m_confirm_ble_key = false;
	if (pResp->has_rgubcBleKey) {
		m_confirm_ble_key = true;
		LOG_INF("Received a ble_sec_key of size %d",
			pResp->rgubcBleKey.size);
		uint8_t current_ble_sec_key[EEP_BLE_SEC_KEY_LEN];
		eep_read_ble_sec_key(current_ble_sec_key, EEP_BLE_SEC_KEY_LEN);
		int ret = memcmp(pResp->rgubcBleKey.bytes, current_ble_sec_key,
				 pResp->rgubcBleKey.size);
		if (ret != 0) {
			LOG_INF("New ble sec key is different. Will update eeprom");
			ret = eep_write_ble_sec_key(pResp->rgubcBleKey.bytes,
						    pResp->rgubcBleKey.size);
			if (ret < 0) {
				char *e_msg =
					"Failed to write ble sec key to EEPROM";
				LOG_ERR("%s (%d)", log_strdup(e_msg), ret);
				nf_app_error(ERR_MESSAGING, ret, e_msg,
					     strlen(e_msg));
			}
		}
	}
	if (pResp->has_versionInfo) {
		process_upgrade_request(&pResp->versionInfo);
	}
	if (pResp->ulFenceDefVersion != current_state.fence_version &&
	    new_fence_in_progress != pResp->ulFenceDefVersion) {
		/* Request frame 0. */
		first_frame = true;
		LOG_INF("Requesting frame 0 for fence version %i.",
			pResp->ulFenceDefVersion);
		int ret = request_fframe(pResp->ulFenceDefVersion, 0);
		if (ret == 0) {
			first_frame = true;
			new_fence_in_progress = pResp->ulFenceDefVersion;
		}
	}
	return;
}

/* @brief: starts a firmware download if a new version exists on the server. */
void process_upgrade_request(VersionInfoFW *fw_ver_from_server)
{
	//uint32_t app_ver = fw_ver_from_server->ulNRF52AppVersion;
	uint32_t app_ver = fw_ver_from_server->ulATmegaVersion;

	LOG_INF("Received app version from server %i", app_ver);

	if (app_ver != NF_X25_VERSION_NUMBER) {
		struct start_fota_event *ev = new_start_fota_event();
		ev->override_default_host = false;
		ev->version = app_ver;
		EVENT_SUBMIT(ev);
	} else {
		LOG_INF("FW ver from server is same or older than current.");
		return;
	}

	return;
}

/** @brief Process a fence frame and stores it into the cached pasture
 *         so we can validate if its valid when we're done to further
 *         store on external flash.
 * 
 * @param fenceResp fence frame received from server.
 * 
 * @returns 0 = requests the first frame again, something happened.
 * @returns 1-254 = frame number processed. This is used
 *          to know which frame to request next.
 * @returns Do we need negative error code if error, I.e pasture
 *          is not valid, or just return 0 and retry forever?
 * @returns 255 if download is complete.
 */
uint8_t process_fence_msg(FenceDefinitionResponse *fenceResp)
{
	if (fenceResp == NULL) {
		return 0;
	}

	uint8_t frame = fenceResp->ucFrameNumber;

	int err = 0;

	if (new_fence_in_progress != fenceResp->ulFenceDefVersion) {
		/* Something went wrong, restart fence request. */
		return 0;
	}

	if (frame == 0) {
		memset(&pasture_temp, 0, sizeof(pasture_t));
		cached_fences_counter = 0;
		pasture_temp.m.us_pasture_crc = EMPTY_FENCE_CRC;
	}

	if (fenceResp->which_m == FenceDefinitionResponse_xHeader_tag) {
		if (frame != 0) {
			/* We always expect the header to be the first frame. */
			char *e_msg =
				"Unexpected frame count for pasture header.";
			LOG_ERR("%s (%d)", log_strdup(e_msg), -EIO);
			nf_app_error(ERR_MESSAGING, -EIO, e_msg, strlen(e_msg));

			return 0;
		}

		/* Pasture header. */
		if (fenceResp->m.xHeader.has_bKeepMode) {
			pasture_temp.m.has_keep_mode = true;
			pasture_temp.m.keep_mode =
				fenceResp->m.xHeader.bKeepMode;
		}

		if (fenceResp->has_usFenceCRC) {
			pasture_temp.m.has_us_pasture_crc = true;
			pasture_temp.m.us_pasture_crc = fenceResp->usFenceCRC;
		}

		pasture_temp.m.l_origin_lat = fenceResp->m.xHeader.lOriginLat;
		pasture_temp.m.l_origin_lon = fenceResp->m.xHeader.lOriginLon;
		pasture_temp.m.us_k_lat = fenceResp->m.xHeader.usK_LAT;
		pasture_temp.m.us_k_lon = fenceResp->m.xHeader.usK_LON;
		pasture_temp.m.ul_fence_def_version =
			fenceResp->ulFenceDefVersion;
		pasture_temp.m.ul_total_fences =
			fenceResp->m.xHeader.ulTotalFences;

	} else if (FenceDefinitionResponse_xFence_tag) {
		/* Fence frame info to store into pasture's fence array. */
		fence_t *loc = &pasture_temp.fences[cached_fences_counter];

		/* Fence header. */
		loc->m.n_points = fenceResp->m.xFence.rgulPoints_count;
		loc->m.us_id = fenceResp->m.xFence.usId;
		loc->m.e_fence_type = fenceResp->m.xFence.eFenceType;
		loc->m.fence_no = fenceResp->m.xFence.fenceNo;

		/* Fence coordinates. */
		memcpy(loc->coordinates, fenceResp->m.xFence.rgulPoints,
		       loc->m.n_points * sizeof(fence_coordinate_t));

		/* Increment number of fences stored in pasture. */
		cached_fences_counter++;
	}

	LOG_INF("Cached fence frame %i successfully.", frame);
	if (frame == fenceResp->ucTotalFrames - 1) {
		/* Validate pasture. */
		if (cached_fences_counter != pasture_temp.m.ul_total_fences) {
			LOG_ERR("Cached %i frames, but expected %i.",
				cached_fences_counter,
				pasture_temp.m.ul_total_fences);
			char *e_msg = "Fence frames out of sync";
			nf_app_error(ERR_MESSAGING, -EIO, e_msg, strlen(e_msg));

			return 0;
		}

		if (pasture_temp.m.ul_total_fences == 0) {
			char *e_msg = "Error, pasture cached is empty.";
			LOG_ERR("%s (%d)", log_strdup(e_msg), -EIO);
			nf_app_error(ERR_MESSAGING, -EIO, e_msg, strlen(e_msg));
			return 0;
		}

		if (!validate_pasture()) {
			char *e_msg = "CRC was not correct for new pasture.";
			LOG_ERR("%s (%d)", log_strdup(e_msg), -EIO);
			nf_app_error(ERR_MESSAGING, -EIO, e_msg, strlen(e_msg));
			return 0;
		}

		LOG_INF("Validated CRC for pasture and will write it to flash.");
		err = stg_write_pasture_data((uint8_t *)&pasture_temp,
					     sizeof(pasture_temp));
		if (err) {
			return err;
		}
		return DOWNLOAD_COMPLETE;
	}

	return frame;
}

uint8_t process_ano_msg(UbxAnoReply *anoResp)
{
	uint8_t rec_ano_frames =
		anoResp->rgucBuf.size / sizeof(UBX_MGA_ANO_RAW_t);
	UBX_MGA_ANO_RAW_t *temp = NULL;
	temp = (UBX_MGA_ANO_RAW_t *)(anoResp->rgucBuf.bytes +
				     sizeof(UBX_MGA_ANO_RAW_t));

	uint32_t age = ano_date_to_unixtime_midday(
		temp->mga_ano.year, temp->mga_ano.month, temp->mga_ano.day);

	LOG_INF("Relative age of received ANO frame = %d, %d", age,
		time_from_server);

	/* Write to storage controller's ANO WRITE partition. */
	int err = stg_write_ano_data((uint8_t *)&anoResp->rgucBuf,
				     anoResp->rgucBuf.size);

	if (err) {
		char *e_msg = "Error writing ano frame to storage controller";
		LOG_ERR("%s (%d)", log_strdup(e_msg), err);
		nf_app_error(ERR_MESSAGING, err, e_msg, strlen(e_msg));
	}

	if (age > time_from_server + SECONDS_IN_THREE_DAYS) {
		return DOWNLOAD_COMPLETE;
	}
	return rec_ano_frames;
}

static void proto_get_last_known_date_pos(gnss_last_fix_struct_t *gpsLastFix,
					  _DatePos *pos)
{
	bool valid_headVeh = (bool)(gpsLastFix->pvt_flags &
				    GPS_UBX_NAV_PVT_VALID_HEADVEH_MASK);
	_DatePos a = {
		.lLat = gpsLastFix->lat,
		.lLon = gpsLastFix->lon,
		.usHorizontalAccDm = gpsLastFix->h_acc_dm,
		.ulUnixTimestamp = gpsLastFix->unix_timestamp,
		.has_sHeadVeh = valid_headVeh,
		.sHeadVeh = gpsLastFix->head_veh,
		.has_usHeadAcc = valid_headVeh,
		.usHeadAcc = gpsLastFix->head_acc,
		.has_ucNumSV = true,
		.ucNumSV = gpsLastFix->num_sv,
		.has_usHDOP = true,
		.usHDOP = gpsLastFix->h_dop,
		.has_lHeight = true,
		.lHeight = gpsLastFix->height,
#if defined(HW_BAROMETER)
		.has_usHeight = true,
		.usHeight = gpsLastFix->baro_Height,
#endif
		.has_ucGpsMode = true,
		.ucGpsMode = gpsLastFix->mode
	};
	memcpy(pos, &a, sizeof(_DatePos));
}

bool proto_has_last_known_date_pos(const gnss_last_fix_struct_t *gps)
{
	return gps->unix_timestamp != 0;
}

static uint32_t ano_date_to_unixtime_midday(uint8_t year, uint8_t month,
					    uint8_t day)
{
	nf_time_t nf_time;
	nf_time.day = day;
	nf_time.month = month;
	nf_time.year = year + 2000;
	nf_time.hour = 12; //assumed per ANO specs
	nf_time.minute = nf_time.second = 0;
	return time2unix(&nf_time);
}
