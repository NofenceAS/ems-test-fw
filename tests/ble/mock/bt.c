#include <ztest.h>
#include <bt.h>

int bt_enable(bt_ready_cb_t cb)
{
	int err = ztest_get_return_value();
	if (cb != NULL) {
		cb(err);
	}
	return err;
}

int bt_le_adv_start(const struct bt_le_adv_param *param,
		    const struct bt_data *ad, size_t ad_len,
		    const struct bt_data *sd, size_t sd_len)
{
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

int bt_le_adv_update_data(const struct bt_data *ad, size_t ad_len,
			  const struct bt_data *sd, size_t sd_len)
{
	return ztest_get_return_value();
}

uint16_t bt_gatt_get_mtu(struct bt_conn *conn)
{
	return ztest_get_return_value();
}

int bt_addr_le_to_str(const bt_addr_le_t *addr, char *str, size_t len)
{
	return ztest_get_return_value();
}

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

int bt_gatt_exchange_mtu(struct bt_conn *conn,
			 struct bt_gatt_exchange_params *params)
{
	return ztest_get_return_value();
}

void bt_conn_cb_register(struct bt_conn_cb *cb)
{
}