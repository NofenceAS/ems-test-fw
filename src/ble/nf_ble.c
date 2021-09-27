//
// Created by per on 27.08.2021.
//

#include <zephyr/types.h>
#include <stddef.h>
#include <sys/printk.h>
#include <sys/util.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <sys/byteorder.h>
#include "nf_ble.h"
#include "beacon.h"
#include "beacon_processor.h"

#define BT_LE_ADV_NCONN_NAME_SLOW                                                                  \
	BT_LE_ADV_PARAM(/*BT_LE_ADV_OPT_CONNECTABLE | */                                           \
			BT_LE_ADV_OPT_USE_NAME, BT_GAP_ADV_SLOW_INT_MIN, BT_GAP_ADV_SLOW_INT_MAX,  \
			NULL)

static uint8_t mfg_data[] = { 0xff, 0xff, 0x00 };

static const struct bt_data ad[] = {
	BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg_data, 3),
};

static bool data_cb(struct bt_data *data, void *user_data)
{
	adv_data_t *adv_data = user_data;
	struct net_buf_simple net_buf;

	if (data->type == BT_DATA_MANUFACTURER_DATA) {
		net_buf_simple_init_with_data(&net_buf, (void *)data->data, data->data_len);
		adv_data->manuf_id = net_buf_simple_pull_be16(&net_buf);
		adv_data->beacon_dev_type = net_buf_simple_pull_u8(&net_buf);
		uint8_t data_len = net_buf_simple_pull_u8(&net_buf);
		if (data_len == BEACON_DATA_LEN) {
			memcpy(&adv_data->uuid.val, net_buf_simple_pull_mem(&net_buf, 16), 16);
			adv_data->major = net_buf_simple_pull_be16(&net_buf);
			adv_data->minor = net_buf_simple_pull_be16(&net_buf);
			adv_data->rssi = net_buf_simple_pull_u8(&net_buf);
			printk("Nofence beacon %u %u %u\n", adv_data->major, adv_data->minor,
			       adv_data->rssi);
		} else {
			memset(adv_data, 0, sizeof(*adv_data));
		}
		return false;

	} else {
		return true;
	}
}

/**
 * @brief Callback for reporting LE scan results.
 *
 * @param addr Advertiser LE address and type.
 * @param rssi Strength of advertiser signal.
 * @param adv_type Type of advertising response from advertiser.
 * @param buf Buffer containing advertiser data.
 */
static void scan_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type,
		    struct net_buf_simple *buf)
{
	adv_data_t adv_data;
	bt_data_parse(buf, data_cb, (void *)&adv_data);
	if (adv_data.major == BEACON_MAJOR_ID && adv_data.minor == BEACON_MINOR_ID) {
		printk("process_event\n");
		const uint32_t now = k_uptime_get_32();
		beac_process_event(now, addr, rssi, &adv_data);
	}
}

void nf_ble_init()
{
	int err;
	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}
	printk("Bluetooth initialized\n");
	/* Start advertising */
	err = bt_le_adv_start(BT_LE_ADV_NCONN_NAME_SLOW, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}
}

void nf_ble_start_scan()
{
	struct bt_le_scan_param scan_param = {
		.type = BT_HCI_LE_SCAN_PASSIVE,
		.options = BT_LE_SCAN_OPT_NONE,
		.interval = 0x003C,
		.window = 0x0028,
	};
	int err;
	err = bt_le_scan_start(&scan_param, scan_cb);
	if (err) {
		printk("Starting scanning failed (err %d)\n", err);
		return;
	}
}
