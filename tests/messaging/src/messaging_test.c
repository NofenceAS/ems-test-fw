/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "messaging.h"
#include "ble_ctrl_event.h"
#include "ble_data_event.h"
#include "cellular_controller_events.h"
#include "messaging_module_events.h"
#include "collar_protocol.h"

static K_SEM_DEFINE(msg_out, 0, 1);
static K_SEM_DEFINE(new_host, 0, 1);

enum test_event_id { TEST_EVENT_CONTENTS = 0, TEST_EVENT_THREAD_POLLER = 1 };
static enum test_event_id cur_id = TEST_EVENT_CONTENTS;

uint8_t *pMsg = NULL;
size_t len;
uint8_t msg_count = 0;
char host[24] = "########################";

/* Provide custom assert post action handler to handle the assertion on OOM
 * error in Event Manager.
 */
BUILD_ASSERT(!IS_ENABLED(CONFIG_ASSERT_NO_FILE_INFO));
void assert_post_action(const char *file, unsigned int line)
{
	printk("assert_post_action - file: %s (line: %u)\n", file, line);
}

void test_init(void)
{
	zassert_false(event_manager_init(),
		      "Error when initializing event manager");
}

/* Store dummy data. */
static uint8_t dummy_data[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
struct ble_ctrl_event dummy_ctrl_event = { .param.battery = 5 };

void test_event_contents(void)
{
	/* Allocate events. */
	struct ble_ctrl_event *ev_ble_ctrl = new_ble_ctrl_event();
	struct ble_data_event *ev_ble_data = new_ble_data_event();
	struct cellular_proto_in_event *ev_lte_proto =
		new_cellular_proto_in_event();

	/* BLE CTRL event. */
	ev_ble_ctrl->cmd = BLE_CTRL_BATTERY_UPDATE;
	ev_ble_ctrl->param.battery = dummy_ctrl_event.param.battery;

	/* BLE data event. */
	ev_ble_data->buf = dummy_data;
	ev_ble_data->len = sizeof(dummy_data);

	/* LTE data event. */
	ev_lte_proto->buf = dummy_data;
	ev_lte_proto->len = sizeof(dummy_data);

	/* Submit events. */
	EVENT_SUBMIT(ev_ble_ctrl);
	EVENT_SUBMIT(ev_ble_data);
	EVENT_SUBMIT(ev_lte_proto);
}

void test_event_poller(void)
{
	/* Allocate events. */
	struct ble_ctrl_event *ev_ble_ctrl = new_ble_ctrl_event();
	struct cellular_proto_in_event *ev_lte_proto =
		new_cellular_proto_in_event();

	/* Submit events. */
	EVENT_SUBMIT(ev_ble_ctrl);
	EVENT_SUBMIT(ev_lte_proto);

	/* Wait for semaphores. */
	int err = k_sem_take(&ble_ctrl_sem, K_SECONDS(30));
	zassert_equal(err, 0, "ble_ctrl_sem hanged.");

	err = k_sem_take(&ble_data_sem, K_SECONDS(30));
	zassert_not_equal(err, 0, "ble_data_sem should hang.");

	err = k_sem_take(&lte_proto_sem, K_SECONDS(30));
	zassert_equal(err, 0, "lte_proto_sem hanged.");
}

void setup_take_semaphores(void)
{
	/* Make sure semaphores are taken so we depend on waiting for them
	 * from the poller thread. Remove this when we have proper
	 * outputs from the messaging module we can monitor.
	 */
	k_sem_take(&ble_ctrl_sem, K_SECONDS(30));
	k_sem_take(&ble_data_sem, K_SECONDS(30));
	k_sem_take(&lte_proto_sem, K_SECONDS(30));
}

void teardown_clear_threads(void)
{
	/* Sleep test thread so other threads can finish what they're doing
	 * before test terminates.
	 */
	k_sleep(K_SECONDS(30));
}

/* Test expected events published by messaging*/
//ack - fence_ready - ano_ready - msg_out - host_address
void test_initial_poll_request_out(void)
{
	zassert_false(event_manager_init(),
		      "Error when initializing event manager\n");

	messaging_module_init();
	k_sem_take(&msg_out, K_SECONDS(0.5));
	printk("Outbound messages = %d\n", msg_count);
	zassert_not_equal(pMsg, NULL, "Proto message not published!\n");
	NofenceMessage decode;
	int err = collar_protocol_decode(pMsg + 2, len - 2, &decode);
	zassert_equal(err, 0, "Corrupt proto message!\n");
	printk("%d\n", decode.which_m);
	zassert_equal(decode.which_m, NofenceMessage_poll_message_req_tag,
		      "Expected "
		      "fence def. "
		      "request- not "
		      "sent!\n");
}

void test_poll_response_has_new_fence(void)
{
	int dummy_fence = 4;
	NofenceMessage decode;
	NofenceMessage poll_response = {
		.which_m = NofenceMessage_poll_message_resp_tag,
		.header = {
			.ulId = 0,
			.ulUnixTimestamp = 1,
			.ulVersion = 0,
			.has_ulVersion = false,
		},
		.m.poll_message_resp = {
			.eActivationMode = ActivationMode_Active,
			.ulFenceDefVersion = dummy_fence,
			.has_bUseUbloxAno = true,
			.bUseUbloxAno = true,
			.has_usPollConnectIntervalSec = false,
			.usPollConnectIntervalSec = 60
		}
	};

	uint8_t encoded_msg[NofenceMessage_size];
	memset(encoded_msg, 0, sizeof(encoded_msg));
	size_t encoded_size = 0;
	int ret = collar_protocol_encode(&poll_response, &encoded_msg[0],
					 sizeof(encoded_msg), &encoded_size);
	zassert_equal(ret, 0, "Could not encode server response!\n");
	memcpy(&encoded_msg[2], &encoded_msg[0], encoded_size);
	struct cellular_proto_in_event *msgIn = new_cellular_proto_in_event();
	msgIn->buf = &encoded_msg[0];
	msgIn->len = encoded_size + 2;
	EVENT_SUBMIT(msgIn);
	k_sem_take(&msg_out, K_FOREVER);
	int err = collar_protocol_decode(pMsg + 2, len - 2, &decode);
	zassert_equal(err, 0, "Decode error!\n");
	printk("%d\n", decode.which_m);
	zassert_equal(decode.which_m, NofenceMessage_fence_definition_req_tag,
		      "Expected "
		      "fence def. "
		      "request- not "
		      "sent!\n");
	zassert_equal(decode.m.fence_definition_req.ulFenceDefVersion,
		      dummy_fence, "Wrong fence version requested!\n");
}

void test_poll_response_has_host_address(void)
{
	ztest_returns_value(stg_read_log_data, 0);
	ztest_returns_value(stg_log_pointing_to_last, true);
	ztest_returns_value(stg_clear_partition, 0);

	//	messaging_module_init();
	char dummy_host[] = "111.222.333.4444:12345";
	//	NofenceMessage decode;
	NofenceMessage poll_response = {
		.which_m = NofenceMessage_poll_message_resp_tag,
		.header = {
			.ulId = 0,
			.ulUnixTimestamp = 1,
			.ulVersion = 0,
			.has_ulVersion = false,
		},
		.m.poll_message_resp = {
			.has_xServerIp = true,
			.eActivationMode = ActivationMode_Active,
			.ulFenceDefVersion = 0,
			.has_bUseUbloxAno = true,
			.bUseUbloxAno = true,
			.has_usPollConnectIntervalSec = false,
			.usPollConnectIntervalSec = 60
		}
	};
	memcpy(poll_response.m.poll_message_resp.xServerIp, dummy_host,
	       sizeof(dummy_host[0] * 22));
	uint8_t encoded_msg[NofenceMessage_size];
	memset(encoded_msg, 0, sizeof(encoded_msg));
	size_t encoded_size = 0;
	int ret = collar_protocol_encode(&poll_response, &encoded_msg[0],
					 sizeof(encoded_msg), &encoded_size);
	zassert_equal(ret, 0, "Could not encode server response!\n");
	memcpy(&encoded_msg[2], &encoded_msg[0], encoded_size);
	struct cellular_proto_in_event *msgIn = new_cellular_proto_in_event();
	msgIn->buf = &encoded_msg[0];
	msgIn->len = encoded_size + 2;
	EVENT_SUBMIT(msgIn);

	ret = k_sem_take(&new_host, K_SECONDS(2));
	zassert_equal(ret, 0, "New host event not published!\n");
	ret = memcmp(host, dummy_host, sizeof(dummy_host[0] * 22));
	zassert_equal(ret, 0, "Host address mismatch!\n");
}

/* Test expected error events published by messaging*/
// time_out waiting for ack from cellular_controller

void test_main(void)
{
	ztest_test_suite(messaging_tests, ztest_unit_test(test_init),
			 ztest_unit_test_setup_teardown(test_event_contents,
							setup_take_semaphores,
							teardown_clear_threads),

			 ztest_unit_test_setup_teardown(test_event_poller,
							setup_take_semaphores,
							teardown_clear_threads),

			 ztest_unit_test(test_initial_poll_request_out),
			 ztest_unit_test(test_poll_response_has_new_fence),
			 ztest_unit_test(test_poll_response_has_host_address));

	ztest_run_test_suite(messaging_tests);
}

static bool event_handler(const struct event_header *eh)
{
	if (cur_id == TEST_EVENT_CONTENTS) {
		if (is_ble_ctrl_event(eh)) {
			struct ble_ctrl_event *ev = cast_ble_ctrl_event(eh);
			switch (ev->cmd) {
			case BLE_CTRL_BATTERY_UPDATE:
				zassert_equal(ev->param.battery,
					      dummy_ctrl_event.param.battery,
					      "Battery update mismatch.");
				break;
			default:
				zassert_unreachable(
					"Unexpected command event.");
				return false;
			}
		}
		if (is_ble_data_event(eh)) {
			struct ble_data_event *ev = cast_ble_data_event(eh);
			zassert_equal(ev->len, sizeof(dummy_data),
				      "Buffer length mismatch");
			zassert_mem_equal(ev->buf, dummy_data, ev->len,
					  "Contents not equal.");
			return false;
		}
		if (is_cellular_proto_in_event(eh)) {
			struct cellular_proto_in_event *ev =
				cast_cellular_proto_in_event(eh);
			zassert_equal(ev->len, sizeof(dummy_data),
				      "Buffer length mismatch");
			zassert_mem_equal(ev->buf, dummy_data, ev->len,
					  "Contents not equal.");
			cur_id = TEST_EVENT_THREAD_POLLER;
			return false;
		}
	}
	if (is_messaging_proto_out_event(eh)) {
		msg_count++;
		struct messaging_proto_out_event *ev =
			cast_messaging_proto_out_event(eh);
		printk("length of encoded message = %d\n", ev->len);
		pMsg = ev->buf;
		len = ev->len;
		k_sem_give(&msg_out);
		return true;
	}
	if (is_messaging_host_address_event(eh)) {
		struct messaging_host_address_event *ev =
			cast_messaging_host_address_event(eh);
		memcpy(host, ev->address, sizeof(ev->address));
		k_sem_give(&new_host);
		return true;
	}
	return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, ble_ctrl_event);
EVENT_SUBSCRIBE(test_main, ble_data_event);
EVENT_SUBSCRIBE(test_main, cellular_proto_in_event);
EVENT_SUBSCRIBE(test_main, messaging_proto_out_event);
EVENT_SUBSCRIBE(test_main, messaging_host_address_event);
