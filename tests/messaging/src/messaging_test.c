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
#include "eeprom.h"

static K_SEM_DEFINE(msg_out, 0, 1);
static K_SEM_DEFINE(new_host, 0, 1);

#define BYTESWAP16(x) (((x) << 8) | ((x) >> 8))

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
	struct connection_state_event *ev
		= new_connection_state_event();
	ev->state = true;
	EVENT_SUBMIT(ev);
	ztest_returns_value(eep_read_serial, 0);
	ztest_returns_value(stg_read_log_data, 0);
	ztest_returns_value(stg_log_pointing_to_last, false);

	zassert_false(event_manager_init(),
		      "Error when initializing event manager");
	messaging_module_init();

	k_sleep(K_SECONDS(10));
}

/* Test expected events published by messaging*/
//ack - fence_ready - ano_ready - msg_out - host_address
void test_initial_poll_request_out(void)
{
	struct connection_state_event *ev
		= new_connection_state_event();
	ev->state = true;
	EVENT_SUBMIT(ev);
	k_sem_take(&msg_out, K_MSEC(500));
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

	k_sleep(K_SECONDS(1));
}

void test_poll_response_has_new_fence(void)
{
	/* We need to simulate that we received the message on server, publish
	 * ACK for messaging module.
	 */
	struct cellular_ack_event *ev = new_cellular_ack_event();
	EVENT_SUBMIT(ev);
	k_sem_take(&msg_out, K_NO_WAIT);
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
	zassert_equal(k_sem_take(&msg_out, K_SECONDS(15)), 0, "");
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

	k_sleep(K_SECONDS(10));
}

void test_poll_response_has_host_address(void)
{
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
	struct connection_state_event *ev
		= new_connection_state_event();
	ev->state = true;
	EVENT_SUBMIT(ev);

	ret = k_sem_take(&new_host, K_SECONDS(2));
	zassert_equal(ret, 0, "New host event not published!\n");
	ret = memcmp(host, dummy_host, sizeof(dummy_host[0] * 22));
	zassert_equal(ret, 0, "Host address mismatch!\n");

	k_sleep(K_SECONDS(1));
}

NofenceMessage dummy_nf_msg = { .m.seq_msg.has_usBatteryVoltage = 1500 };
static bool encode_test = false;

void test_encode_message(void)
{
	encode_test = true;
	k_sem_take(&msg_out, K_NO_WAIT);

	/* We need to simulate that we received the message on server, publish
	 * ACK for messaging module.
	 */
	struct cellular_ack_event *ev = new_cellular_ack_event();
	EVENT_SUBMIT(ev);

	int ret = encode_and_send_message(&dummy_nf_msg);
	zassert_equal(ret, 0, "");

	printk("ret %i\n", ret);
	zassert_equal(k_sem_take(&msg_out, K_SECONDS(30)), 0, "");

	k_sleep(K_SECONDS(1));
}

/* Test expected error events published by messaging*/
// time_out waiting for ack from cellular_controller

void test_main(void)
{
	ztest_test_suite(messaging_tests, ztest_unit_test(test_init),
			 ztest_unit_test(test_initial_poll_request_out),
			 ztest_unit_test(test_poll_response_has_new_fence),
			 ztest_unit_test(test_poll_response_has_host_address),
			 ztest_unit_test(test_encode_message));

	ztest_run_test_suite(messaging_tests);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_messaging_proto_out_event(eh)) {
		msg_count++;
		struct messaging_proto_out_event *ev =
			cast_messaging_proto_out_event(eh);
		if (!encode_test) {
			printk("length of encoded message = %d\n", ev->len);
			pMsg = ev->buf;
			len = ev->len;
		} else {
			NofenceMessage msg;
			int ret = collar_protocol_decode(&ev->buf[2],
							 ev->len - 2, &msg);
			zassert_equal(ret, 0, "");

			uint16_t byteswap_val = BYTESWAP16(ev->len - 2);
			uint16_t expected_val;
			memcpy(&expected_val, &ev->buf[0], 2);
			zassert_equal(byteswap_val, expected_val, "");
		}
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
EVENT_SUBSCRIBE(test_main, messaging_proto_out_event);
EVENT_SUBSCRIBE(test_main, messaging_host_address_event);
