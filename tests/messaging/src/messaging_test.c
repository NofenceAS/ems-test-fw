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
#include "nf_version.h"
#include "gnss_controller_events.h"
#include "storage_event.h"
#include "histogram_events.h"
#include "error_event.h"

static K_SEM_DEFINE(msg_out, 0, 1);
static K_SEM_DEFINE(msg_in, 0, 1);
static K_SEM_DEFINE(new_host, 0, 1);
static K_SEM_DEFINE(error_sem, 0, 1);

#define BYTESWAP16(x) (((x) << 8) | ((x) >> 8))

uint8_t *pMsg = NULL;
size_t len;
uint8_t msg_count = 0;
char host[24] = "########################";
static bool simulated_connection_state = true;
static bool cellular_ack_ok = true;

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
	struct connection_state_event *ev = new_connection_state_event();
	ev->state = true;
	EVENT_SUBMIT(ev);

	ztest_returns_value(stg_config_u16_read, 0);
	ztest_returns_value(stg_config_u16_read, 0);
	ztest_returns_value(stg_config_u16_read, 0);

	ztest_returns_value(stg_config_u8_read, 0);
	ztest_returns_value(stg_config_u8_read, 0);
	ztest_returns_value(stg_config_u8_read, 0);
	ztest_returns_value(stg_config_u8_read, 0);
	ztest_returns_value(stg_config_blob_read, 0);
	ztest_returns_value(stg_config_u8_read, 0);
	ztest_returns_value(stg_config_u16_read, 0);
	ztest_returns_value(stg_config_u32_read, 0);
	ztest_returns_value(date_time_now, 0);
	ztest_returns_value(stg_config_u8_read, 0);
	ztest_returns_value(stg_config_u8_write, 0);

	/* Cache variables for messaging module. */
	struct gnss_data *ev_gnss = new_gnss_data();
	EVENT_SUBMIT(ev_gnss);

	struct update_collar_mode *ev_cmode = new_update_collar_mode();
	EVENT_SUBMIT(ev_cmode);

	struct update_collar_status *ev_cstatus = new_update_collar_status();
	EVENT_SUBMIT(ev_cstatus);

	struct update_fence_status *ev_fstatus = new_update_fence_status();
	EVENT_SUBMIT(ev_fstatus);

	struct update_fence_version *ev_fversion = new_update_fence_version();
	EVENT_SUBMIT(ev_fversion);

	struct update_zap_count *ev_zap = new_update_zap_count();
	EVENT_SUBMIT(ev_zap);

	struct gsm_info_event *ev_gsm = new_gsm_info_event();
	EVENT_SUBMIT(ev_gsm);


	zassert_false(event_manager_init(),
		      "Error when initializing event manager");
	messaging_module_init();

	k_sleep(K_SECONDS(0.1));


	/* the build_log function is scheduled after 25 seconds,
	 * will only send out messages from the flash -if any since
	 * m_transfer_boot_params is true until we get the initial poll
	 * response from the server.*/
	ztest_returns_value(stg_read_log_data, 0);
	ztest_returns_value(stg_log_pointing_to_last, false);
	k_sleep(K_SECONDS(25));
}

/* Test expected events published by messaging*/
//ack - fence_ready - ano_ready - msg_out - host_address
int poll_interval =15;
void test_second_poll_request_has_no_boot_parameters(void)
{
	/*assumes 15min poll interval, 25sec delay for build_log work
	* checks: second poll request sent out without the boot parameters*/
	ztest_returns_value(date_time_now, 0);
	/* TODO pshustad, pending if we are going to use the connection_state_event for poll */

	NofenceMessage decode;
	int err = collar_protocol_decode(pMsg + 2, len - 2, &decode);

	zassert_equal(err, 0, "Corrupt proto message!\n");
	zassert_equal(decode.which_m, NofenceMessage_poll_message_req_tag,
		      "Wrong message");
	zassert_true(decode.m.poll_message_req.has_versionInfo, "");
	zassert_true(
		decode.m.poll_message_req.versionInfo.has_ulApplicationVersion,
		"");
	zassert_equal(
		decode.m.poll_message_req.versionInfo.ulApplicationVersion,
		NF_X25_VERSION_NUMBER, "");
	zassert_false(decode.m.poll_message_req.versionInfo.has_ulATmegaVersion,
		      "");
	zassert_false(
		decode.m.poll_message_req.versionInfo.has_ulNRF52AppVersion,
		"");
	zassert_false(decode.m.poll_message_req.versionInfo
			      .has_ulNRF52BootloaderVersion,
		      "");
	zassert_false(decode.m.poll_message_req.versionInfo
			      .has_ulNRF52SoftDeviceVersion,
		      "");

	/* Simulate a poll-reply to reset the send_bootloader_paramters */
	NofenceMessage poll_response = {
		.which_m = NofenceMessage_poll_message_resp_tag,
		.header = {
			.ulId = 0,
			.ulUnixTimestamp = 1,
		},
		.m.poll_message_resp = {
			.eActivationMode = ActivationMode_Active,
			.ulFenceDefVersion = 0
		}
	};
	uint8_t encoded_msg[NofenceMessage_size];
	memset(encoded_msg, 0, sizeof(encoded_msg));
	size_t encoded_size;

	int ret = collar_protocol_encode(&poll_response, &encoded_msg[0],
					 sizeof(encoded_msg), &encoded_size);
	zassert_equal(ret, 0, "Could not encode server response!\n");
	memcpy(&encoded_msg[2], &encoded_msg[0], encoded_size);
	struct cellular_proto_in_event *msgIn = new_cellular_proto_in_event();
	msgIn->buf = &encoded_msg[0];
	msgIn->len = encoded_size + 2;
	EVENT_SUBMIT(msgIn);

	/* wait for fake poll reply to be processed */
	zassert_equal(k_sem_take(&msg_in, K_MSEC(500)), 0, "");
	/* Now test the second poll message */
	pMsg = NULL;

	k_sem_reset(&msg_out);
	k_sleep(K_MINUTES(poll_interval));

	zassert_equal(k_sem_take(&msg_out, K_MSEC(500)), 0, "");
	zassert_not_equal(pMsg, NULL, "Proto message not published!\n");
	err = collar_protocol_decode(pMsg + 2, len - 2, &decode);
	zassert_equal(err, 0, "Corrupt proto message!\n");
	zassert_equal(decode.which_m, NofenceMessage_poll_message_req_tag,
		      "Wrong message");
	zassert_false(decode.m.poll_message_req.has_versionInfo, "");

}

void test_poll_request_out_when_nudged_from_server(void)
{
	ztest_returns_value(date_time_now, 0);
	k_sleep(K_SECONDS(5));

	struct send_poll_request_now *wake_up = new_send_poll_request_now();
	EVENT_SUBMIT(wake_up);

	k_sem_take(&msg_out, K_MSEC(500));
	printk("Outbound messages = %d\n", msg_count);
	zassert_not_equal(pMsg, NULL, "Proto message not published!\n");

	NofenceMessage decode;
	int err = collar_protocol_decode(pMsg + 2, len - 2, &decode);

	zassert_equal(err, 0, "Corrupt proto message!\n");
	printk("%d\n", decode.which_m);
	zassert_equal(decode.which_m, NofenceMessage_poll_message_req_tag,
		      "Expected poll request not sent!\n");

	zassert_equal(decode.which_m, NofenceMessage_poll_message_req_tag,
		      "Wrong message type, poll request expected!");
	zassert_false(decode.m.poll_message_req.has_versionInfo, "");
}

void test_poll_response_has_new_fence(void)
{
	ztest_returns_value(date_time_now, 0);

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
		      "Expected fence def. request- not sent!");
	zassert_equal(decode.m.fence_definition_req.ulFenceDefVersion,
		      dummy_fence, "Wrong fence version requested!\n");
	zassert_equal(decode.m.fence_definition_req.ucFrameNumber,
		      0, "Wrong fence frame number requested!\n");

	ztest_returns_value(date_time_now, 0);
	NofenceMessage fence_response = {
		.which_m = NofenceMessage_fence_definition_resp_tag,
		.header = {
			.ulId = 0,
			.ulUnixTimestamp = 2,
			.ulVersion = 1,
			.has_ulVersion = true
		},
		.m.fence_definition_resp =
			{
				.ulFenceDefVersion = dummy_fence,
				.ucFrameNumber = 0,
				.ucTotalFrames = 7,
				.m.xHeader = {
					.lOriginLat = 1,
					.lOriginLon = 2,
					.lOriginLon = 3,
					.usK_LAT = 4,
					.usK_LON = 5,
					.has_bKeepMode = false
				},
			},
	};

	memset(encoded_msg, 0, sizeof(encoded_msg));
	encoded_size = 0;
	ret = collar_protocol_encode(&fence_response, &encoded_msg[0],
				     sizeof(encoded_msg), &encoded_size);
	zassert_equal(ret, 0, "Could not encode server response!\n");
	printk("encoded size = %d \n", encoded_size);
	memcpy(&encoded_msg[2], &encoded_msg[0], encoded_size);
	struct cellular_proto_in_event *fence_msgIn =
		new_cellular_proto_in_event();
	fence_msgIn->buf = &encoded_msg[0];
	fence_msgIn->len = encoded_size + 2;
	EVENT_SUBMIT(fence_msgIn);

	zassert_equal(k_sem_take(&msg_out, K_SECONDS(15)), 0, "");
	err = collar_protocol_decode(pMsg + 2, len - 2, &decode);
	zassert_equal(err, 0, "Decode error!\n");
	printk("%d\n", decode.which_m);
	zassert_equal(decode.which_m, NofenceMessage_fence_definition_req_tag,
		      "Expected fence def. request- not sent!");
	zassert_equal(decode.m.fence_definition_req.ulFenceDefVersion,
		      dummy_fence, "Wrong fence version requested!\n");
	zassert_equal(decode.m.fence_definition_req.ucFrameNumber,
		      1, "Wrong fence frame number requested!\n");

	k_sleep(K_SECONDS(0.5));
	ztest_returns_value(date_time_now, 0);
	dummy_fence = 5;
	NofenceMessage poll_response2 = {
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

	memset(encoded_msg, 0, sizeof(encoded_msg));
	encoded_size = 0;
	ret = collar_protocol_encode(&poll_response2, &encoded_msg[0],
					 sizeof(encoded_msg), &encoded_size);
	zassert_equal(ret, 0, "Could not encode server response!\n");

	memcpy(&encoded_msg[2], &encoded_msg[0], encoded_size);
	struct cellular_proto_in_event *msgIn2 = new_cellular_proto_in_event();
	msgIn2->buf = &encoded_msg[0];
	msgIn2->len = encoded_size + 2;
	EVENT_SUBMIT(msgIn2);

	zassert_equal(k_sem_take(&msg_out, K_SECONDS(15)), 0, "");
	err = collar_protocol_decode(pMsg + 2, len - 2, &decode);
	zassert_equal(err, 0, "Decode error!\n");
	printk("%d\n", decode.which_m);
	zassert_equal(decode.which_m, NofenceMessage_fence_definition_req_tag,
		      "Expected fence def. request- not sent!");
	zassert_equal(decode.m.fence_definition_req.ulFenceDefVersion,
		      dummy_fence, "Wrong fence version requested!\n");
	zassert_equal(decode.m.fence_definition_req.ucFrameNumber,
		      0, "Wrong fence frame number requested!\n");
}

void test_poll_response_has_host_address(void)
{
	char dummy_host[] = "111.222.333.4444:12345";

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

void test_poll_request_retry_after_missing_ack_from_cellular_controller(void)
{
	/*assumes 15min poll interval, 25sec delay for build_log work */
	cellular_ack_ok = false;
	k_sem_reset(&error_sem);
	/* log_work thread start*/
	collar_histogram dummy_histogram;
	memset(&dummy_histogram, 1, sizeof(struct collar_histogram));
	while (k_msgq_put(&histogram_msgq, &dummy_histogram,
			  K_NO_WAIT) != 0) {
		k_msgq_purge(&histogram_msgq);
	}
	ztest_returns_value(stg_write_log_data, 0);
	ztest_returns_value(date_time_now, 0);
	ztest_returns_value(stg_write_log_data, 0);
	ztest_returns_value(date_time_now, 0);
	ztest_returns_value(stg_read_log_data, 0);
	ztest_returns_value(stg_log_pointing_to_last, false);
	/* log_work thread end*/

	/* poll request #1: no cellular ack will be received for this one*/
	ztest_returns_value(date_time_now, 0);

	k_sem_reset(&msg_out);

	/* poll request #2-  retry after 1 minute*/
	ztest_returns_value(date_time_now, 0);
	k_sleep(K_MINUTES(1+poll_interval));

	zassert_equal(k_sem_take(&msg_out, K_MSEC(500)), 0, "");

	zassert_equal(k_sem_take(&error_sem, K_SECONDS
				 (CONFIG_CC_ACK_TIMEOUT_SEC)), 0, "");

	NofenceMessage decode;
	cellular_ack_ok = true;
	zassert_not_equal(pMsg, NULL, "Proto message not published!\n");
	int err = collar_protocol_decode(pMsg + 2, len - 2, &decode);
	zassert_equal(err, 0, "Corrupt proto message!\n");
	zassert_equal(decode.which_m, NofenceMessage_poll_message_req_tag,
		      "Wrong message");
	zassert_false(decode.m.poll_message_req.has_versionInfo, "");
}

void test_main(void)
{
	ztest_test_suite(
		messaging_tests, ztest_unit_test(test_init),
		ztest_unit_test(test_second_poll_request_has_no_boot_parameters),
		ztest_unit_test(test_poll_request_out_when_nudged_from_server),
		ztest_unit_test(test_poll_response_has_new_fence),
		ztest_unit_test(test_poll_response_has_host_address)
		,ztest_unit_test
				(test_poll_request_retry_after_missing_ack_from_cellular_controller)
		);

	ztest_run_test_suite(messaging_tests);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_messaging_proto_out_event(eh)) {
		printk("msg_count: %d\n", msg_count++);
		struct messaging_proto_out_event *ev =
			cast_messaging_proto_out_event(eh);
		pMsg = ev->buf;
		len = ev->len;
		NofenceMessage msg;
		int ret = collar_protocol_decode(&ev->buf[2],
						 ev->len - 2, &msg);
		printk("ret = %d\n", ret);
		zassert_equal(ret, 0, "");
		uint16_t byteswap_val = BYTESWAP16(ev->len - 2);
		uint16_t expected_val;
		memcpy(&expected_val, &ev->buf[0], 2);
		zassert_equal(byteswap_val, expected_val, "");
		k_sem_give(&msg_out);
		if (cellular_ack_ok) {
			struct cellular_ack_event *ack = new_cellular_ack_event();
			ack->message_sent = true;
			EVENT_SUBMIT(ack);
		} else {
			struct cellular_ack_event *ack = new_cellular_ack_event();
			ack->message_sent = false;
			EVENT_SUBMIT(ack);
		}
		printk("Simulated cellular ack!\n");
		k_sleep(K_SECONDS(1));

		return true;
	}
	if (is_messaging_host_address_event(eh)) {
		struct messaging_host_address_event *ev =
			cast_messaging_host_address_event(eh);
		memcpy(host, ev->address, sizeof(ev->address));
		k_sem_give(&new_host);
		return true;
	}
	if (is_messaging_ack_event(eh)) {
		k_sem_give(&msg_in);
	}
	if (is_error_event(eh)) {
		k_sem_give(&error_sem);
		return false;
	}
	if (is_check_connection(eh)) {
		struct connection_state_event *ev1 = new_connection_state_event();
		ev1->state = simulated_connection_state;
		EVENT_SUBMIT(ev1);
		return false;
	}
	return false;
}

EVENT_LISTENER(test_main, event_handler);
EVENT_SUBSCRIBE(test_main, messaging_proto_out_event);
EVENT_SUBSCRIBE(test_main, messaging_ack_event);
EVENT_SUBSCRIBE(test_main, messaging_host_address_event);
EVENT_SUBSCRIBE(test_main, error_event);
EVENT_SUBSCRIBE(test_main, check_connection);
