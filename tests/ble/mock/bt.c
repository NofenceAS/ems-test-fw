#include <ztest.h>
#include <bt.h>
#include <zephyr.h>

#define BUFFER_AD_LEN 5
struct bt_data buffer_ad[BUFFER_AD_LEN];

static bt_ready_cb_t ready_cb;
static int ready_cb_err;

/**
 * @brief simulates time taken by the BLE stack to initialize
 */
static void delayed_cb_fn(struct k_work *work)
{
	ready_cb(ready_cb_err);
}

static K_WORK_DELAYABLE_DEFINE(delayed_cb, delayed_cb_fn);

/**
 *  @brief Simulated bt_enable with some delay fired on a separate thread if the
 *  cb is non null
 */
int bt_enable(bt_ready_cb_t cb)
{
	int err = ztest_get_return_value();
	ready_cb = cb;
	ready_cb_err = err;
	if (cb != NULL) {
		k_work_schedule(&delayed_cb, K_SECONDS(1));
	}
	return err;
}

bool bt_mock_is_battery_adv_data_correct(uint8_t data)
{
	if (data == buffer_ad[BLE_AD_IDX_MANUFACTURER].data[BLE_MFG_IDX_BATTERY]) {
		return 1;
	}
	return 0;
}

bool bt_mock_is_error_flag_adv_data_correct(uint8_t data)
{
	if (data == buffer_ad[BLE_AD_IDX_MANUFACTURER].data[BLE_MFG_IDX_ERROR]) {
		return 1;
	}
	return 0;
}

bool bt_mock_is_collar_mode_adv_data_correct(uint8_t data)
{
	if (data == buffer_ad[BLE_AD_IDX_MANUFACTURER].data[BLE_MFG_IDX_COLLAR_MODE]) {
		return 1;
	}
	return 0;
}

bool bt_mock_is_collar_status_adv_data_correct(uint8_t data)
{
	if (data == buffer_ad[BLE_AD_IDX_MANUFACTURER].data[BLE_MFG_IDX_COLLAR_STATUS]) {
		return 1;
	}
	return 0;
}

bool bt_mock_is_fence_status_adv_data_correct(uint8_t data)
{
	if (data == buffer_ad[BLE_AD_IDX_MANUFACTURER].data[BLE_MFG_IDX_FENCE_STATUS]) {
		return 1;
	}
	return 0;
}

bool bt_mock_is_pasture_status_adv_data_correct(uint8_t data)
{
	if (data == buffer_ad[BLE_AD_IDX_MANUFACTURER].data[BLE_MFG_IDX_VALID_PASTURE]) {
		return 1;
	}
	return 0;
}

bool bt_mock_is_fence_def_ver_adv_data_correct(uint16_t data)
{
	if (data == buffer_ad[BLE_AD_IDX_MANUFACTURER].data[BLE_MFG_IDX_FENCE_DEF_VER]) {
		return 1;
	}
	return 0;
}

int bt_le_adv_start(const struct bt_le_adv_param *param, const struct bt_data *ad, size_t ad_len,
		    const struct bt_data *sd, size_t sd_len)
{
	if (ad_len > BUFFER_AD_LEN) {
		return -ENOMEM;
	}
	memcpy(buffer_ad, ad, ad_len * sizeof(struct bt_data));
	return ztest_get_return_value();
}

int bt_nus_send(struct bt_conn *conn, const uint8_t *data, uint16_t len)
{
	return ztest_get_return_value();
}

int bt_le_adv_stop(void)
{
	return ztest_get_return_value();
}

int bt_le_adv_update_data(const struct bt_data *ad, size_t ad_len, const struct bt_data *sd,
			  size_t sd_len)
{
	if (ad_len > BUFFER_AD_LEN) {
		return -ENOMEM;
	}
	memcpy(buffer_ad, ad, ad_len * sizeof(struct bt_data));
	return ztest_get_return_value();
}

uint16_t bt_gatt_get_mtu(struct bt_conn *conn)
{
	return ztest_get_return_value();
}

// int bt_addr_le_to_str(const bt_addr_le_t *addr, char *str, size_t len)
// {
// 	return ztest_get_return_value();
// }

int bt_nus_init(struct bt_nus_cb *callbacks)
{
	return ztest_get_return_value();
}

const bt_addr_le_t *bt_conn_get_dst(const struct bt_conn *conn)
{
	return ztest_get_return_value_ptr();
}

void bt_conn_unref(struct bt_conn *conn)
{
}

struct bt_conn *bt_conn_ref(struct bt_conn *conn)
{
	return ztest_get_return_value_ptr();
}

int bt_gatt_exchange_mtu(struct bt_conn *conn, struct bt_gatt_exchange_params *params)
{
	return ztest_get_return_value();
}

void bt_conn_cb_register(struct bt_conn_cb *cb)
{
}

void bt_data_parse(struct net_buf_simple *ad, bool (*func)(struct bt_data *data, void *user_data),
		   void *user_data)
{
}

void net_buf_simple_init_with_data(struct net_buf_simple *buf, void *data, size_t size)
{
}
uint16_t net_buf_simple_pull_be16(struct net_buf_simple *buf)
{
	return ztest_get_return_value();
}

uint8_t net_buf_simple_pull_u8(struct net_buf_simple *buf)
{
	return ztest_get_return_value();
}

void *net_buf_simple_pull_mem(struct net_buf_simple *buf, size_t len)
{
	void *data = buf->data;
	return data;
}

int bt_le_scan_start(const struct bt_le_scan_param *param, bt_le_scan_cb_t cb)
{
	return ztest_get_return_value();
}

int bt_le_scan_stop(void)
{
	return ztest_get_return_value();
}

int bt_conn_disconnect(struct bt_conn *conn, uint8_t reason)
{
	return ztest_get_return_value();
}

int bt_set_name(const char *name)
{
	return ztest_get_return_value();
}