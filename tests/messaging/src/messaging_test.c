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
#include "fw_upgrade_events.h"
#include "embedded.pb.h"

static K_SEM_DEFINE(msg_out, 0, 1);
static K_SEM_DEFINE(seq_msg_out, 0, 1);
static K_SEM_DEFINE(msg_in, 0, 1);
static K_SEM_DEFINE(new_host, 0, 1);
static K_SEM_DEFINE(error_sem, 0, 1);

#define BYTESWAP16(x) (((x) << 8) | ((x) >> 8))

NofenceMessage m_latest_proto_msg;

uint8_t *pMsg = NULL;
size_t len;
uint8_t msg_count = 0;

static int m_poll_interval = 15;
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
	/* 
	 * Test the initialization of the messaging module. Verify that the first poll request sent 
	 * after boot and initialization contains boot parameters.
	 */

	struct connection_state_event *ev = new_connection_state_event();
	ev->state = true;
	EVENT_SUBMIT(ev);

	/* Init UID */
	ztest_returns_value(stg_config_u32_read, -1);

	/* Poll Req. w/ boot parameters */
	ztest_returns_value(date_time_now, 0); //Protobuf header
	ztest_returns_value(stg_config_u16_read, 0);
	ztest_returns_value(stg_config_u16_read, -1);
	ztest_returns_value(stg_config_u16_read, 0);
	ztest_returns_value(stg_config_blob_read, 0);
	ztest_returns_value(stg_config_u8_read, 0);
	ztest_returns_value(stg_config_u16_read, 0);
	ztest_returns_value(stg_config_u8_read, -1);
	ztest_returns_value(stg_config_u8_read, -1);
	ztest_returns_value(stg_config_u8_read, 0);
	ztest_returns_value(stg_config_u8_read, 0);
	ztest_returns_value(stg_config_u8_read, 0); //Boot reason
	ztest_returns_value(stg_config_u8_write, 0); //Boot reason

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

	zassert_false(event_manager_init(), "Error when initializing event manager");
	zassert_false(messaging_module_init(), "Error when initializing messaging module");

	/* Verify that poll request is sent */
	k_sem_reset(&msg_out);
	zassert_equal(k_sem_take(&msg_out, K_SECONDS(15)), 0, "");

	/* Verify initial poll req. content and boot parameters */
	zassert_equal(m_latest_proto_msg.which_m, NofenceMessage_poll_message_req_tag, 
		      "");
	zassert_equal(m_latest_proto_msg.m.poll_message_req.versionInfo.ulApplicationVersion, 
		      NF_X25_VERSION_NUMBER, 
		      "");
	zassert_true(m_latest_proto_msg.m.poll_message_req.has_versionInfo, 
		     "");
	zassert_true(m_latest_proto_msg.m.poll_message_req.versionInfo.has_ulApplicationVersion, 
		     "");
	zassert_false(m_latest_proto_msg.m.poll_message_req.versionInfo.has_ulATmegaVersion, 
		      "");
	zassert_false(m_latest_proto_msg.m.poll_message_req.versionInfo.has_ulNRF52AppVersion, 
		      "");
	zassert_false(m_latest_proto_msg.m.poll_message_req.versionInfo.has_ulNRF52BootloaderVersion, 
		      "");
	zassert_false(m_latest_proto_msg.m.poll_message_req.versionInfo.has_ulNRF52SoftDeviceVersion, 
		      "");

	zassert_equal(m_latest_proto_msg.header.ulId, 1, "");

	/* Verify that seq message is NOT sent. Periodic seq message is scheduled after 30 minutes 
	 * and should not store and send messages before receiving the initial poll response from 
	 * the server- which is does not get in this test case */
	k_sleep(K_SECONDS(30));
}

void test_second_poll_request_has_no_boot_parameters(void)
{
	/* 
	 * Test that the the second (and consecutive) poll request after the initial boot poll 
	 * request does NOT include boot parameters.
	 * 
	 * Note! This assumes a 15 min poll request interval.
	 */

	/* Simulate a poll-reply to the initial poll request. This will reset the 
	 * transfer_boot_params flag in the messaging module. */
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

	int ret = collar_protocol_encode(&poll_response, &encoded_msg[0], sizeof(encoded_msg), 
					 &encoded_size);
	zassert_equal(ret, 0, "Could not encode server response!");

	k_sem_reset(&msg_in);

	memcpy(&encoded_msg[2], &encoded_msg[0], encoded_size);
	struct cellular_proto_in_event *msgIn = new_cellular_proto_in_event();
	msgIn->buf = &encoded_msg[0];
	msgIn->len = encoded_size + 2;
	EVENT_SUBMIT(msgIn);

	/* Wait for fake poll reply to be processed */
	zassert_equal(k_sem_take(&msg_in, K_MSEC(500)), 0, "");

	/* Now test the second poll message */
	k_sem_reset(&msg_out);
	ztest_returns_value(date_time_now, 0); //Build poll req.

	k_sleep(K_MINUTES(m_poll_interval));

	zassert_equal(k_sem_take(&msg_out, K_MSEC(500)), 0, "");
	zassert_equal(m_latest_proto_msg.which_m, NofenceMessage_poll_message_req_tag, "");
	zassert_false(m_latest_proto_msg.m.poll_message_req.has_versionInfo, "");
}

void test_send_seq_message_with_preceding_poll_req(void)
{
	/* 
	 * Test that SEQ messages are sent at the appropriate time and that the sending of SEQ
	 * messages are preceded by a poll request. Seq. messages will never reach here due to 
	 * being hardware dependent (seq. are not sent for empty external flash), but test will
	 * fail by ztest return values if periodic seq. messages are not sent. Also, verify the
	 * log message poll request.
	 *
	 * Note! This assumes a 30 min seq message interval.
	 */

	/* Check the initial message count before starting the test */
	zassert_equal(msg_count, 2, "");

	ztest_returns_value(date_time_now, 0); //Poll request

	/* Seq message */
	collar_histogram dummy_histogram;
	memset(&dummy_histogram, 1, sizeof(struct collar_histogram));
	while (k_msgq_put(&histogram_msgq, &dummy_histogram, K_NO_WAIT) != 0) {
		k_msgq_purge(&histogram_msgq);
	}
	ztest_returns_value(stg_write_log_data, 0);
	ztest_returns_value(date_time_now, 0);
	ztest_returns_value(stg_write_log_data, 0);

	ztest_returns_value(date_time_now, 0);
	ztest_returns_value(stg_read_log_data, 0);
	ztest_returns_value(stg_log_pointing_to_last, false);

	/* Wait for the periodic seq message to trigger */
	k_sleep(K_MINUTES(m_poll_interval + 1));

	zassert_equal(k_sem_take(&msg_out, K_MSEC(500)), 0, "");

	zassert_equal(msg_count, 4 /* Initial count + log preceding poll req. */, "");
}

void test_stop_excessive_poll_requests(void)
{
	/* Test that log messages, which normally sends an preceding poll request, do NOT send a poll 
	 * request when sent within 1 minute of a poll request, periodic or otherwise */

	/* Send poll request */
	k_sem_reset(&msg_out);
	ztest_returns_value(date_time_now, 0);
	struct send_poll_request_now *poll_evt = new_send_poll_request_now();
	EVENT_SUBMIT(poll_evt);

	zassert_equal(k_sem_take(&msg_out, K_SECONDS(1)), 0, "");
	zassert_equal(m_latest_proto_msg.which_m, NofenceMessage_poll_message_req_tag, "");

	/* Send a log message (animal escaped) immediately after a poll request */
	ztest_returns_value(date_time_now, 0); //Proto header log msg.
	ztest_returns_value(stg_write_log_data, 0); //Store log msg
	ztest_returns_value(stg_read_log_data, 0); //Send log msg
	ztest_returns_value(stg_log_pointing_to_last, false); //Send log msg
	struct animal_escape_event *escaped_evt = new_animal_escape_event();
	EVENT_SUBMIT(escaped_evt);

	/* Wait for ~1 minute */
	k_sleep(K_SECONDS(62));

	/* Send a log message (animal escaped) over 1 minute after a poll request */
	k_sem_reset(&msg_out);
	ztest_returns_value(date_time_now, 0); //Proto header Poll req.
	ztest_returns_value(date_time_now, 0); //Proto header Log msg.
	ztest_returns_value(stg_write_log_data, 0); //Store message
	ztest_returns_value(stg_read_log_data, 0); //Send log msg
	ztest_returns_value(stg_log_pointing_to_last, false); //Send log msg
	struct animal_escape_event *escaped_evt1 = new_animal_escape_event();
	EVENT_SUBMIT(escaped_evt1);

	k_sleep(K_SECONDS(1));

	zassert_equal(k_sem_take(&msg_out, K_SECONDS(1)), 0, "");
	zassert_equal(m_latest_proto_msg.which_m, NofenceMessage_poll_message_req_tag, "");
}

void test_poll_request_out_when_nudged_from_server(void)
{
	/*
	 * Test that the send_poll_request_now event, sent by a nudge from the server, triggers the
	 * messaging module to send a poll request to the server immediately.
	 */
	ztest_returns_value(date_time_now, 0); //Build poll req.
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
	zassert_false(decode.m.poll_message_req.has_versionInfo, "");
}

void test_poll_response_has_new_fence(void)
{
	/* 
	 * Test that a new fence avilable from the server (as indicated in a poll response) is 
	 * handled correctly in the messaging module. 
	 */
	ztest_returns_value(date_time_now, 0);

	/* Simulate a poll-reply that also indicate that a new fence is available to download */
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
	int ret = collar_protocol_encode(&poll_response, &encoded_msg[0], sizeof(encoded_msg), 
					 &encoded_size);
	zassert_equal(ret, 0, "Could not encode server response!\n");

	memcpy(&encoded_msg[2], &encoded_msg[0], encoded_size);
	struct cellular_proto_in_event *msgIn = new_cellular_proto_in_event();
	msgIn->buf = &encoded_msg[0];
	msgIn->len = encoded_size + 2;
	EVENT_SUBMIT(msgIn);

	/* Wait for an verify fence definition request message */
	zassert_equal(k_sem_take(&msg_out, K_SECONDS(15)), 0, "");
	int err = collar_protocol_decode(pMsg + 2, len - 2, &decode);
	zassert_equal(err, 0, "Decode error!\n");
	printk("%d\n", decode.which_m);
	zassert_equal(decode.which_m, NofenceMessage_fence_definition_req_tag,
		      "Expected fence def. request- not sent!");
	zassert_equal(decode.m.fence_definition_req.ulFenceDefVersion, dummy_fence, 
		      "Wrong fence version requested!\n");
	zassert_equal(decode.m.fence_definition_req.ucFrameNumber, 0, 
		      "Wrong fence frame number requested!\n");

	/* Simulate a poll-reply to the fence definition request */
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
	ret = collar_protocol_encode(&fence_response, &encoded_msg[0], sizeof(encoded_msg), 
				     &encoded_size);
	zassert_equal(ret, 0, "Could not encode server response!\n");
	printk("encoded size = %d \n", encoded_size);
	memcpy(&encoded_msg[2], &encoded_msg[0], encoded_size);
	struct cellular_proto_in_event *fence_msgIn = new_cellular_proto_in_event();
	fence_msgIn->buf = &encoded_msg[0];
	fence_msgIn->len = encoded_size + 2;
	EVENT_SUBMIT(fence_msgIn);

	/* Wait for an verify fence definition request message */
	zassert_equal(k_sem_take(&msg_out, K_SECONDS(15)), 0, "");
	err = collar_protocol_decode(pMsg + 2, len - 2, &decode);
	zassert_equal(err, 0, "Decode error!\n");
	printk("%d\n", decode.which_m);
	zassert_equal(decode.which_m, NofenceMessage_fence_definition_req_tag,
		      "Expected fence def. request- not sent!");
	zassert_equal(decode.m.fence_definition_req.ulFenceDefVersion, dummy_fence, 
		      "Wrong fence version requested!\n");
	zassert_equal(decode.m.fence_definition_req.ucFrameNumber, 1, 
		      "Wrong fence frame number requested!\n");

	k_sleep(K_SECONDS(0.5));

	/* Simulate a poll-reply to the fence definition request */
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
	ret = collar_protocol_encode(&poll_response2, &encoded_msg[0], sizeof(encoded_msg), 
				     &encoded_size);
	zassert_equal(ret, 0, "Could not encode server response!\n");

	memcpy(&encoded_msg[2], &encoded_msg[0], encoded_size);
	struct cellular_proto_in_event *msgIn2 = new_cellular_proto_in_event();
	msgIn2->buf = &encoded_msg[0];
	msgIn2->len = encoded_size + 2;
	EVENT_SUBMIT(msgIn2);

	/* Wait for an verify fence definition request message */
	zassert_equal(k_sem_take(&msg_out, K_SECONDS(15)), 0, "");
	err = collar_protocol_decode(pMsg + 2, len - 2, &decode);
	zassert_equal(err, 0, "Decode error!\n");
	printk("%d\n", decode.which_m);
	zassert_equal(decode.which_m, NofenceMessage_fence_definition_req_tag,
		      "Expected fence def. request- not sent!");
	zassert_equal(decode.m.fence_definition_req.ulFenceDefVersion, dummy_fence, 
		      "Wrong fence version requested!\n");
	zassert_equal(decode.m.fence_definition_req.ucFrameNumber, 0, 
		      "Wrong fence frame number requested!\n");
}

void test_fence_download_frame_loss(void)
{
	/* 
	 * Test that a new fence avilable from the server (as indicated in a poll response) is 
	 * downloaded and that a packet loss (i.e. incorrect frame number in download process) will 
	 * reset the fence download request. 
	 */
	int ret = 0;
	size_t encoded_size = 0;
	uint8_t encoded_msg[NofenceMessage_size];

	struct cellular_proto_in_event *fence_request_msg_evt;
	struct cellular_proto_in_event *poll_resp_msg_evt;

	/* Simulate a poll response that indicate that a new fence is available to download */
	NofenceMessage poll_resp = {
		.which_m = NofenceMessage_poll_message_resp_tag,
		.header = {
			.ulId = 0,
			.ulUnixTimestamp = 1,
			.ulVersion = 0,
			.has_ulVersion = false,
		},
		.m.poll_message_resp = {
			.eActivationMode = ActivationMode_Active,
			.ulFenceDefVersion = 10 /* New fence def. version = 10 */,
			.has_bUseUbloxAno = true,
			.bUseUbloxAno = true,
			.has_usPollConnectIntervalSec = false,
			.usPollConnectIntervalSec = 60
		}
	};
	memset(encoded_msg, 0, sizeof(encoded_msg));
	ret = collar_protocol_encode(&poll_resp, &encoded_msg[0], sizeof(encoded_msg), 
				     &encoded_size);
	zassert_equal(ret, 0, "Could not encode server response");

	k_sem_reset(&msg_out);
	ztest_returns_value(date_time_now, 0);

	memcpy(&encoded_msg[2], &encoded_msg[0], encoded_size);
	poll_resp_msg_evt = new_cellular_proto_in_event();
	poll_resp_msg_evt->buf = &encoded_msg[0];
	poll_resp_msg_evt->len = encoded_size + 2;
	EVENT_SUBMIT(poll_resp_msg_evt);

	/* Verify that fence definition request message is sent for frame 0 of version 10. */
	zassert_equal(k_sem_take(&msg_out, K_SECONDS(5)), 0, "");
	zassert_equal(m_latest_proto_msg.which_m, NofenceMessage_fence_definition_req_tag, "");
	zassert_equal(m_latest_proto_msg.m.fence_definition_req.ulFenceDefVersion, 10, "");
	zassert_equal(m_latest_proto_msg.m.fence_definition_req.ucFrameNumber, 
		      0 /* Requesting frame 0 (header) */, "");

	/* Simulate a poll response to the fence definition request for fence header */
	NofenceMessage fence_resp = {
		.which_m = NofenceMessage_fence_definition_resp_tag,
		.header = {
			.ulId = 0,
			.ulUnixTimestamp = 2,
			.ulVersion = 1,
			.has_ulVersion = true
		},
		.m.fence_definition_resp = {
			.which_m = FenceDefinitionResponse_xHeader_tag,
			.ulFenceDefVersion = 10,
			.ucFrameNumber = 0,
			.ucTotalFrames = 5,
			.m.xHeader = {
				.ulTotalFences = 4,
				.lOriginLat = 1,
				.lOriginLon = 2,
				.lOriginLon = 3,
				.usK_LAT = 4,
				.usK_LON = 5,
				.has_bKeepMode = false
			},
		},
	};
	ret = collar_protocol_encode(&fence_resp, &encoded_msg[0], sizeof(encoded_msg), 
				     &encoded_size);
	zassert_equal(ret, 0, "Could not encode server response");

	k_sem_reset(&msg_out);
	ztest_returns_value(date_time_now, 0);

	memcpy(&encoded_msg[2], &encoded_msg[0], encoded_size);
	fence_request_msg_evt = new_cellular_proto_in_event();
	fence_request_msg_evt->buf = &encoded_msg[0];
	fence_request_msg_evt->len = encoded_size + 2;
	EVENT_SUBMIT(fence_request_msg_evt);

	/* Verify that fence definition request message is sent for frame 1/5 of version 10. */
	zassert_equal(k_sem_take(&msg_out, K_SECONDS(5)), 0, "");
	zassert_equal(m_latest_proto_msg.which_m, NofenceMessage_fence_definition_req_tag,"");
	zassert_equal(m_latest_proto_msg.m.fence_definition_req.ulFenceDefVersion, 10, "");
	zassert_equal(m_latest_proto_msg.m.fence_definition_req.ucFrameNumber, 
		      1 /* Requesting frame 1 (1st xFence frame) */, "");

	/* Simulate a poll response to the fence definition request for frame 1 */
	fence_resp.m.fence_definition_resp.which_m = FenceDefinitionResponse_xFence_tag;
	fence_resp.m.fence_definition_resp.ucFrameNumber = 1;

	ret = collar_protocol_encode(&fence_resp, &encoded_msg[0], sizeof(encoded_msg), 
				     &encoded_size);
	zassert_equal(ret, 0, "Could not encode server response");

	k_sem_reset(&msg_out);
	ztest_returns_value(date_time_now, 0);

	memcpy(&encoded_msg[2], &encoded_msg[0], encoded_size);
	fence_request_msg_evt = new_cellular_proto_in_event();
	fence_request_msg_evt->buf = &encoded_msg[0];
	fence_request_msg_evt->len = encoded_size + 2;
	EVENT_SUBMIT(fence_request_msg_evt);

	/* Verify that fence definition request message is sent for frame 2/5 of version 10 */
	zassert_equal(k_sem_take(&msg_out, K_SECONDS(5)), 0, "");
	zassert_equal(m_latest_proto_msg.which_m, NofenceMessage_fence_definition_req_tag,"");
	zassert_equal(m_latest_proto_msg.m.fence_definition_req.ulFenceDefVersion, 10, "");
	zassert_equal(m_latest_proto_msg.m.fence_definition_req.ucFrameNumber, 2, "");

	/* Simulate a poll response to the fence definition request of frame no. 3. Meaning that 
	 * frame no. 2 has been lost. This should reset the fence download in the messaging module,
	 * thus no fence request message will return from this- only a poll response with a fence 
	 * version different than the current one will restart the fence download */
	fence_resp.m.fence_definition_resp.ucFrameNumber = 3 /* Messaging expects no. 2 */;

	ret = collar_protocol_encode(&fence_resp, &encoded_msg[0], sizeof(encoded_msg), 
				     &encoded_size);
	zassert_equal(ret, 0, "Could not encode server response");

	k_sem_reset(&msg_out);

	memcpy(&encoded_msg[2], &encoded_msg[0], encoded_size);
	fence_request_msg_evt = new_cellular_proto_in_event();
	fence_request_msg_evt->buf = &encoded_msg[0];
	fence_request_msg_evt->len = encoded_size + 2;
	EVENT_SUBMIT(fence_request_msg_evt);

	/* Verify that fence definition request message is NOT sent */
	zassert_not_equal(k_sem_take(&msg_out, K_SECONDS(5)), 0, "");


	/* Send a simulated poll response and verify that a new fence request download of version 10
	 * starts from frame 0 and continues untill download is completed, i.e. no packet loss */
	ret = collar_protocol_encode(&poll_resp, &encoded_msg[0], sizeof(encoded_msg), 
				     &encoded_size);
	zassert_equal(ret, 0, "Could not encode server response");

	k_sem_reset(&msg_out);
	ztest_returns_value(date_time_now, 0);

	memcpy(&encoded_msg[2], &encoded_msg[0], encoded_size);
	poll_resp_msg_evt = new_cellular_proto_in_event();
	poll_resp_msg_evt->buf = &encoded_msg[0];
	poll_resp_msg_evt->len = encoded_size + 2;
	EVENT_SUBMIT(poll_resp_msg_evt);

	for (int i = 0; i < fence_resp.m.fence_definition_resp.ucTotalFrames; i++) {
		/* Verify that fence request message is sent */
		zassert_equal(k_sem_take(&msg_out, K_SECONDS(5)), 0, "");
		zassert_equal(m_latest_proto_msg.which_m, NofenceMessage_fence_definition_req_tag, 
			      "");
		zassert_equal(m_latest_proto_msg.m.fence_definition_req.ulFenceDefVersion, 10, 
			      "");
		zassert_equal(m_latest_proto_msg.m.fence_definition_req.ucFrameNumber, i, 
			      "");

		/* Simulate a poll response to the fence definition request of frame i */
		if (i == 0) {
			fence_resp.m.fence_definition_resp.which_m = 
				FenceDefinitionResponse_xHeader_tag;
		} else {
			fence_resp.m.fence_definition_resp.which_m = 
				FenceDefinitionResponse_xFence_tag;
		}
		fence_resp.m.fence_definition_resp.ucFrameNumber = i;

		ret = collar_protocol_encode(&fence_resp, &encoded_msg[0], sizeof(encoded_msg), 
					     &encoded_size);
		zassert_equal(ret, 0, "Could not encode server response");

		k_sem_reset(&msg_out);
		if (i < (fence_resp.m.fence_definition_resp.ucTotalFrames - 1)) {
			ztest_returns_value(date_time_now, 0);
		}

		memcpy(&encoded_msg[2], &encoded_msg[0], encoded_size);
		fence_request_msg_evt = new_cellular_proto_in_event();
		fence_request_msg_evt->buf = &encoded_msg[0];
		fence_request_msg_evt->len = encoded_size + 2;
		EVENT_SUBMIT(fence_request_msg_evt);
	}
	zassert_not_equal(k_sem_take(&msg_out, K_SECONDS(5)), 0, "");
}

void test_poll_response_has_host_address(void)
{
	/*
	 * Test that a new host port avilable from the server (as indicated by the poll response)
	 * is handled correctly in the messaging module.
	 */

	char dummy_host[] = "111.222.333.4444:12345";

	/* Simulate a poll-reply that has a new host port available */
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
	memcpy(poll_response.m.poll_message_resp.xServerIp, dummy_host, sizeof(dummy_host[0] * 22));
	uint8_t encoded_msg[NofenceMessage_size];
	memset(encoded_msg, 0, sizeof(encoded_msg));
	size_t encoded_size = 0;
	int ret = collar_protocol_encode(&poll_response, &encoded_msg[0], sizeof(encoded_msg), 
					 &encoded_size);
	zassert_equal(ret, 0, "Could not encode server response!\n");
	memcpy(&encoded_msg[2], &encoded_msg[0], encoded_size);
	struct cellular_proto_in_event *msgIn = new_cellular_proto_in_event();
	msgIn->buf = &encoded_msg[0];
	msgIn->len = encoded_size + 2;
	EVENT_SUBMIT(msgIn);

	/* Verify that a new host port is handled and set */
	ret = k_sem_take(&new_host, K_SECONDS(2));
	zassert_equal(ret, 0, "New host event not published!\n");
	ret = memcmp(host, dummy_host, sizeof(dummy_host[0] * 22));
	zassert_equal(ret, 0, "Host address mismatch!\n");
}

void test_poll_request_retry_after_missing_ack_from_cellular_controller(void)
{
	NofenceMessage decode;
	/*
	 * Test that check that a poll request is re-attempted after 1 minute if it failed to send
	 * due to missing ACK from the cellular controller.
	 * 
	 * Note! This assumes a 15 min poll request interval.
	 */

	/* Poll request #1: No cellular ack will be received for this one */
	ztest_returns_value(date_time_now, 0);

	cellular_ack_ok = false;
	k_sem_reset(&error_sem);
	k_sem_reset(&msg_out);
	pMsg = NULL;
	struct send_poll_request_now *poll_evt = new_send_poll_request_now();
	EVENT_SUBMIT(poll_evt);

	zassert_equal(k_sem_take(&msg_out, K_MSEC(500)), 0, "");
	zassert_equal(k_sem_take(&error_sem, K_SECONDS(CONFIG_CC_ACK_TIMEOUT_SEC)), 0, "");
	zassert_not_equal(pMsg, NULL, "Proto message not published!\n");
	int err = collar_protocol_decode(pMsg + 2, len - 2, &decode);
	zassert_equal(err, 0, "Corrupt proto message!\n");

	/* Poll request #2: Retry after 1 minute */
	ztest_returns_value(date_time_now, 0);

	cellular_ack_ok = true;
	k_sem_reset(&msg_out);
	k_sem_reset(&error_sem);

	k_sleep(K_MINUTES(1));

	zassert_equal(k_sem_take(&msg_out, K_MSEC(500)), 0, "");
	zassert_not_equal(k_sem_take(&error_sem, K_SECONDS(CONFIG_CC_ACK_TIMEOUT_SEC)), 0, "");


	zassert_not_equal(pMsg, NULL, "Proto message not published!\n");
	err = collar_protocol_decode(pMsg + 2, len - 2, &decode);
	zassert_equal(err, 0, "Corrupt proto message!\n");
	zassert_equal(decode.which_m, NofenceMessage_poll_message_req_tag, "Wrong message");
	zassert_false(decode.m.poll_message_req.has_versionInfo, "");
}

void test_messages_during_fota(void)
{
	/*
	 * Test that during a FOTA only poll requests are sent. Poll responses should not be handled
	 * and other message, such as log messages or ANO data, should NOT be sent.
	 */

	/* Simulate FOTA started/ongoing from the fw_upgrade module by sending the event */
	struct dfu_status_event *fota_start_event = new_dfu_status_event();
	fota_start_event->dfu_status = DFU_STATUS_IN_PROGRESS;
	EVENT_SUBMIT(fota_start_event);

	k_sleep(K_MSEC(500));
	
	/* Send poll request and verify that it is sent during FOTA */
	k_sem_reset(&msg_out);
	ztest_returns_value(date_time_now, 0);
	struct send_poll_request_now *poll_evt = new_send_poll_request_now();
	EVENT_SUBMIT(poll_evt);

	zassert_equal(k_sem_take(&msg_out, K_SECONDS(1)), 0, "");
	zassert_equal(m_latest_proto_msg.which_m, NofenceMessage_poll_message_req_tag, "");

	/* Send a Animal Escaped event in an attempt to force a log message to be sent- which it 
	 * should NOT do during FOTA.
	 * Note! This fails on missing return statements if the message is sent.
	 */
	ztest_returns_value(date_time_now, 0); //Proto header
	ztest_returns_value(stg_write_log_data, 0); //Store message
	struct animal_escape_event *escaped_evt = new_animal_escape_event();
	EVENT_SUBMIT(escaped_evt);

	k_sleep(K_SECONDS(1));

	/* Send a ZAP event in an attempt to force a log message to be sent- which it should NOT do
	 * during FOTA.
	 * Note! This fails on missing return statement if the message is sent.
	 */
	ztest_returns_value(date_time_now, 0);
	ztest_returns_value(stg_write_log_data, 0); //Store message
	struct amc_zapped_now_event *zap_evt = new_amc_zapped_now_event();
	zap_evt->fence_dist = 1;
	EVENT_SUBMIT(zap_evt);

	k_sleep(K_SECONDS(1));
	
	/* Simulate a incomming poll response to verify that it is not processed during FOTA.
	 * Note! This fails on missing return statements if the message is sent.
	 */
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
			.ulFenceDefVersion = 10,
			.has_bUseUbloxAno = true,
			.bUseUbloxAno = true,
			.has_usPollConnectIntervalSec = false,
			.usPollConnectIntervalSec = 60
		}
	};

	uint8_t encoded_msg[NofenceMessage_size];
	memset(encoded_msg, 0, sizeof(encoded_msg));
	size_t encoded_size = 0;

	int ret = collar_protocol_encode(&poll_response, &encoded_msg[0], sizeof(encoded_msg), 
					 &encoded_size);
	zassert_equal(ret, 0, "Unable to decode protobuf message");

	memcpy(&encoded_msg[2], &encoded_msg[0], encoded_size);

	struct cellular_proto_in_event *msgIn = new_cellular_proto_in_event();
	msgIn->buf = &encoded_msg[0];
	msgIn->len = encoded_size + 2;
	EVENT_SUBMIT(msgIn);

	k_sleep(K_SECONDS(1));

	/* Send poll request and verify that it is sent during FOTA */
	k_sem_reset(&msg_out);
	ztest_returns_value(date_time_now, 0);
	struct send_poll_request_now *poll_evt2 = new_send_poll_request_now();
	EVENT_SUBMIT(poll_evt2);

	zassert_equal(k_sem_take(&msg_out, K_SECONDS(1)), 0, "");
	zassert_equal(m_latest_proto_msg.which_m, NofenceMessage_poll_message_req_tag, "");

	/* Simulate FOTA stopped from the fw_upgrade module by sending the event */
	struct dfu_status_event *fota_stop_event = new_dfu_status_event();
	fota_stop_event->dfu_status = DFU_STATUS_IDLE;
	EVENT_SUBMIT(fota_stop_event);

	k_sleep(K_MSEC(500));

	/* Send poll request and verify that it is sent */
	k_sem_reset(&msg_out);
	ztest_returns_value(date_time_now, 0);
	struct send_poll_request_now *poll_evt3 = new_send_poll_request_now();
	EVENT_SUBMIT(poll_evt3);

	zassert_equal(k_sem_take(&msg_out, K_SECONDS(1)), 0, "");
	zassert_equal(m_latest_proto_msg.which_m, NofenceMessage_poll_message_req_tag, "");

	k_sleep(K_SECONDS(1));

	/* Send a Animal Escaped event and verify that the log message is sent now that FOTA has 
	 * been stopped.
	 */
	k_sem_reset(&msg_out);
	ztest_returns_value(stg_write_log_data, 0); //Store message
	ztest_returns_value(date_time_now, 0); //Preceding poll header
	ztest_returns_value(stg_read_log_data, 0); //Send log msg
	ztest_returns_value(stg_log_pointing_to_last, false); //Send log msg

	struct animal_escape_event *escaped_evt1 = new_animal_escape_event();
	EVENT_SUBMIT(escaped_evt1);

	k_sleep(K_SECONDS(1)); //Delay to let the test finish
}

void test_main(void)
{
	ztest_test_suite(messaging_tests,
			 /* Note! Has to be 1st test due to timing */ 
			 ztest_unit_test(test_init),
			 /* Note! Has to be 2nd test due to timing */
			 ztest_unit_test(test_second_poll_request_has_no_boot_parameters),
			 /* Note! Has to be 3rd test due to timing */
			 ztest_unit_test(test_send_seq_message_with_preceding_poll_req),
			 ztest_unit_test(test_stop_excessive_poll_requests),
			 ztest_unit_test(test_poll_request_out_when_nudged_from_server),
			 ztest_unit_test(test_poll_response_has_new_fence),
			 ztest_unit_test(test_fence_download_frame_loss),
			 ztest_unit_test(test_poll_response_has_host_address),
			 ztest_unit_test(test_poll_request_retry_after_missing_ack_from_cellular_controller),
			 ztest_unit_test(test_messages_during_fota)
			);

	ztest_run_test_suite(messaging_tests);
}

static uint8_t buf[NofenceMessage_size +10];
static bool event_handler(const struct event_header *eh)
{
	if (is_messaging_proto_out_event(eh)) {
		printk("msg_count: %d\n", msg_count++);

		struct messaging_proto_out_event *ev = cast_messaging_proto_out_event(eh);
		/* When running with profiling enabled, the ev->buf get corrupted, because it is reused by another thread */
		/* So copy it over to our private buffer */
		memcpy(buf,ev->buf,ev->len);
		pMsg = buf;
		len = ev->len;

		int ret = collar_protocol_decode(&ev->buf[2], ev->len - 2, &m_latest_proto_msg);
		printk("collar_protocol_decode ret = %d\n", ret);
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
		struct messaging_host_address_event *ev = cast_messaging_host_address_event(eh);
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
