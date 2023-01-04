/*
 * Copyright (c) 2021 Nofence AS
 */

#ifndef _BT_H_
#define _BT_H_

#include "event_manager.h"
#include "ble_controller.h"
#include <zephyr.h>
#include <bluetooth/addr.h>

#define CONFIG_BT_DEVICE_NAME "NF999999"

typedef void (*bt_ready_cb_t)(int err);

struct bt_conn {
	uint16_t handle;
	uint8_t type;
	uint8_t role;
};

struct bt_data {
	uint8_t type;
	uint8_t data_len;
	const uint8_t *data;
};

struct bt_le_scan_param {
	/** Scan type (BT_LE_SCAN_TYPE_ACTIVE or BT_LE_SCAN_TYPE_PASSIVE) */
	uint8_t type;

	/** Bit-field of scanning options. */
	uint32_t options;

	/** Scan interval (N * 0.625 ms) */
	uint16_t interval;

	/** Scan window (N * 0.625 ms) */
	uint16_t window;

	/**
	 * @brief Scan timeout (N * 10 ms)
	 *
	 * Application will be notified by the scan timeout callback.
	 * Set zero to disable timeout.
	 */
	uint16_t timeout;

	/**
	 * @brief Scan interval LE Coded PHY (N * 0.625 MS)
	 *
	 * Set zero to use same as LE 1M PHY scan interval.
	 */
	uint16_t interval_coded;

	/**
	 * @brief Scan window LE Coded PHY (N * 0.625 MS)
	 *
	 * Set zero to use same as LE 1M PHY scan window.
	 */
	uint16_t window_coded;
};

// typedef struct {
// 	uint8_t val[6];
// } bt_addr_t;

// /** Bluetooth LE Device Address */
// typedef struct {
// 	uint8_t type;
// 	bt_addr_t a;
// } bt_addr_le_t;

struct bt_le_conn_param {
	uint16_t interval_min;
	uint16_t interval_max;
	uint16_t latency;
	uint16_t timeout;
};

struct bt_le_adv_param {
	/**
	 * @brief Local identity.
	 *
	 * @note When extended advertising @kconfig{CONFIG_BT_EXT_ADV} is not
	 *       enabled or not supported by the controller it is not possible
	 *       to scan and advertise simultaneously using two different
	 *       random addresses.
	 */
	uint8_t id;

	/**
	 * @brief Advertising Set Identifier, valid range 0x00 - 0x0f.
	 *
	 * @note Requires @ref BT_LE_ADV_OPT_EXT_ADV
	 **/
	uint8_t sid;

	/**
	 * @brief Secondary channel maximum skip count.
	 *
	 * Maximum advertising events the advertiser can skip before it must
	 * send advertising data on the secondary advertising channel.
	 *
	 * @note Requires @ref BT_LE_ADV_OPT_EXT_ADV
	 */
	uint8_t secondary_max_skip;

	/** Bit-field of advertising options */
	uint32_t options;

	/** Minimum Advertising Interval (N * 0.625 milliseconds)
	 * Minimum Advertising Interval shall be less than or equal to the
	 * Maximum Advertising Interval. The Minimum Advertising Interval and
	 * Maximum Advertising Interval should not be the same value (as stated
	 * in Bluetooth Core Spec 5.2, section 7.8.5)
	 * Range: 0x0020 to 0x4000
	 */
	uint32_t interval_min;

	/** Maximum Advertising Interval (N * 0.625 milliseconds)
	 * Minimum Advertising Interval shall be less than or equal to the
	 * Maximum Advertising Interval. The Minimum Advertising Interval and
	 * Maximum Advertising Interval should not be the same value (as stated
	 * in Bluetooth Core Spec 5.2, section 7.8.5)
	 * Range: 0x0020 to 0x4000
	 */
	uint32_t interval_max;

	/**
	 * @brief Directed advertising to peer
	 *
	 * When this parameter is set the advertiser will send directed
	 * advertising to the remote device.
	 *
	 * The advertising type will either be high duty cycle, or low duty
	 * cycle if the BT_LE_ADV_OPT_DIR_MODE_LOW_DUTY option is enabled.
	 * When using @ref BT_LE_ADV_OPT_EXT_ADV then only low duty cycle is
	 * allowed.
	 *
	 * In case of connectable high duty cycle if the connection could not
	 * be established within the timeout the connected() callback will be
	 * called with the status set to @ref BT_HCI_ERR_ADV_TIMEOUT.
	 */
	const bt_addr_le_t *peer;
};

enum bt_nus_send_status {
	/** Send notification enabled. */
	BT_NUS_SEND_STATUS_ENABLED,
	/** Send notification disabled. */
	BT_NUS_SEND_STATUS_DISABLED,
};

struct bt_nus_cb {
	/** @brief Data received callback.
	 *
	 * The data has been received as a write request on the NUS RX
	 * Characteristic.
	 *
	 * @param[in] conn  Pointer to connection object that has received data.
	 * @param[in] data  Received data.
	 * @param[in] len   Length of received data.
	 */
	void (*received)(struct bt_conn *conn, const uint8_t *const data, uint16_t len);

	/** @brief Data sent callback.
	 *
	 * The data has been sent as a notification and written on the NUS TX
	 * Characteristic.
	 *
	 * @param[in] conn Pointer to connection object, or NULL if sent to all
	 *                 connected peers.
	 */
	void (*sent)(struct bt_conn *conn);

	/** @brief Send state callback.
	 *
	 * Indicate the CCCD descriptor status of the NUS TX characteristic.
	 *
	 * @param[in] status Send notification status.
	 */
	void (*send_enabled)(enum bt_nus_send_status status);
};

struct bt_conn_cb {
	/** @brief A new connection has been established.
	 *
	 *  This callback notifies the application of a new connection.
	 *  In case the err parameter is non-zero it means that the
	 *  connection establishment failed.
	 *
	 *  @note If the connection was established from an advertising set then
	 *        the advertising set cannot be restarted directly from this
	 *        callback. Instead use the connected callback of the
	 *        advertising set.
	 *
	 *  @param conn New connection object.
	 *  @param err HCI error. Zero for success, non-zero otherwise.
	 *
	 *  @p err can mean either of the following:
	 *  - @ref BT_HCI_ERR_UNKNOWN_CONN_ID Creating the connection started by
	 *    @ref bt_conn_le_create was canceled either by the user through
	 *    @ref bt_conn_disconnect or by the timeout in the host through
	 *    @ref bt_conn_le_create_param timeout parameter, which defaults to
	 *    @kconfig{CONFIG_BT_CREATE_CONN_TIMEOUT} seconds.
	 *  - @p BT_HCI_ERR_ADV_TIMEOUT High duty cycle directed connectable
	 *    advertiser started by @ref bt_le_adv_start failed to be connected
	 *    within the timeout.
	 */
	void (*connected)(struct bt_conn *conn, uint8_t err);

	/** @brief A connection has been disconnected.
	 *
	 *  This callback notifies the application that a connection
	 *  has been disconnected.
	 *
	 *  When this callback is called the stack still has one reference to
	 *  the connection object. If the application in this callback tries to
	 *  start either a connectable advertiser or create a new connection
	 *  this might fail because there are no free connection objects
	 *  available.
	 *  To avoid this issue it is recommended to either start connectable
	 *  advertise or create a new connection using @ref k_work_submit or
	 *  increase @kconfig{CONFIG_BT_MAX_CONN}.
	 *
	 *  @param conn Connection object.
	 *  @param reason HCI reason for the disconnection.
	 */
	void (*disconnected)(struct bt_conn *conn, uint8_t reason);

	/** @brief LE connection parameter update request.
	 *
	 *  This callback notifies the application that a remote device
	 *  is requesting to update the connection parameters. The
	 *  application accepts the parameters by returning true, or
	 *  rejects them by returning false. Before accepting, the
	 *  application may also adjust the parameters to better suit
	 *  its needs.
	 *
	 *  It is recommended for an application to have just one of these
	 *  callbacks for simplicity. However, if an application registers
	 *  multiple it needs to manage the potentially different
	 *  requirements for each callback. Each callback gets the
	 *  parameters as returned by previous callbacks, i.e. they are not
	 *  necessarily the same ones as the remote originally sent.
	 *
	 *  If the application does not have this callback then the default
	 *  is to accept the parameters.
	 *
	 *  @param conn Connection object.
	 *  @param param Proposed connection parameters.
	 *
	 *  @return true to accept the parameters, or false to reject them.
	 */
	bool (*le_param_req)(struct bt_conn *conn, struct bt_le_conn_param *param);

	/** @brief The parameters for an LE connection have been updated.
	 *
	 *  This callback notifies the application that the connection
	 *  parameters for an LE connection have been updated.
	 *
	 *  @param conn Connection object.
	 *  @param interval Connection interval.
	 *  @param latency Connection latency.
	 *  @param timeout Connection supervision timeout.
	 */
	void (*le_param_updated)(struct bt_conn *conn, uint16_t interval, uint16_t latency,
				 uint16_t timeout);

	struct bt_conn_cb *_next;
};

struct bt_gatt_exchange_params {
	/** Response callback */
	void (*func)(struct bt_conn *conn, uint8_t err, struct bt_gatt_exchange_params *params);
};

const bt_addr_le_t *bt_conn_get_dst(const struct bt_conn *conn);

void bt_conn_unref(struct bt_conn *conn);

int bt_enable(bt_ready_cb_t cb);

int bt_le_adv_start(const struct bt_le_adv_param *param, const struct bt_data *ad, size_t ad_len,
		    const struct bt_data *sd, size_t sd_len);

int bt_nus_init(struct bt_nus_cb *callbacks);

int bt_nus_send(struct bt_conn *conn, const uint8_t *data, uint16_t len);

int bt_le_adv_stop(void);

int bt_le_adv_update_data(const struct bt_data *ad, size_t ad_len, const struct bt_data *sd,
			  size_t sd_len);

uint16_t bt_gatt_get_mtu(struct bt_conn *conn);

//int bt_addr_le_to_str(const bt_addr_le_t *addr, char *str, size_t len);

const bt_addr_le_t *bt_conn_get_dst(const struct bt_conn *conn);

void bt_conn_unref(struct bt_conn *conn);

struct bt_conn *bt_conn_ref(struct bt_conn *conn);

int bt_gatt_exchange_mtu(struct bt_conn *conn, struct bt_gatt_exchange_params *params);

void bt_conn_cb_register(struct bt_conn_cb *cb);

struct net_buf_simple {
	/** Pointer to the start of data in the buffer. */
	uint8_t *data;

	/**
	 * Length of the data behind the data pointer.
	 *
	 * To determine the max length, use net_buf_simple_max_len(), not #size!
	 */
	uint16_t len;

	/** Amount of data that net_buf_simple#__buf can store. */
	uint16_t size;

	/** Start of the data storage. Not to be accessed directly
	 *  (the data pointer should be used instead).
	 */
	uint8_t *__buf;
};

void net_buf_simple_init_with_data(struct net_buf_simple *buf, void *data, size_t size);

uint16_t net_buf_simple_pull_be16(struct net_buf_simple *buf);
uint8_t net_buf_simple_pull_u8(struct net_buf_simple *buf);
void *net_buf_simple_pull_mem(struct net_buf_simple *buf, size_t len);

void bt_data_parse(struct net_buf_simple *ad, bool (*func)(struct bt_data *data, void *user_data),
		   void *user_data);

bool bt_mock_is_battery_adv_data_correct(uint8_t data);
bool bt_mock_is_error_flag_adv_data_correct(uint8_t data);
bool bt_mock_is_collar_mode_adv_data_correct(uint8_t data);
bool bt_mock_is_collar_status_adv_data_correct(uint8_t data);
bool bt_mock_is_fence_status_adv_data_correct(uint8_t data);
bool bt_mock_is_pasture_status_adv_data_correct(uint8_t data);
bool bt_mock_is_fence_def_ver_adv_data_correct(uint16_t data);

typedef void bt_le_scan_cb_t(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type,
			     struct net_buf_simple *buf);

int bt_le_scan_start(const struct bt_le_scan_param *param, bt_le_scan_cb_t cb);
int bt_le_scan_stop(void);

int bt_conn_disconnect(struct bt_conn *conn, uint8_t reason);

int bt_set_name(const char *name);
#endif