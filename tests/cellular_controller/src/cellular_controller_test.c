/*
 * Copyright (c) 2022 Nofence AS
 */

#include <ztest.h>
#include "cellular_controller.h"
#include "cellular_controller_events.h"
#include "messaging_module_events.h"
#include "mock_cellular_helpers.h"
#include "error_event.h"
#include "pwr_event.h"

/* semaphores to check publishing of the cellular controller events. */
static K_SEM_DEFINE(download_complete_sem, 0, 1);
static K_SEM_DEFINE(cellular_ack, 0, 1);
static K_SEM_DEFINE(cellular_proto_in, 0, 1);
static K_SEM_DEFINE(cellular_error, 0, 1);
static K_SEM_DEFINE(wake_up, 0, 1);
static K_SEM_DEFINE(send_out_sem, 0, 1);
static K_SEM_DEFINE(stop_connection_sem, 0, 1);
static K_SEM_DEFINE(connection_ready_sem, 0, 1);
static K_SEM_DEFINE(connection_not_ready_sem, 0, 1);

bool simulate_modem_down = false;
extern struct k_sem listen_sem;

void reset_test_semaphores(void)
{
	k_sem_reset(&cellular_ack);
	k_sem_reset(&cellular_proto_in);
	k_sem_reset(&cellular_error);
	k_sem_reset(&wake_up);
	k_sem_reset(&send_out_sem);
	k_sem_reset(&stop_connection_sem);
	k_sem_reset(&download_complete_sem);
}

void test_init(void)
{
	reset_test_semaphores();
	struct pwr_status_event *batt_ev = new_pwr_status_event();
	batt_ev->pwr_state = PWR_NORMAL;
	batt_ev->battery_mv = 4;
	batt_ev->battery_mv_max = 4;
	batt_ev->battery_mv = 4;
	EVENT_SUBMIT(batt_ev);

	struct check_connection *ev = new_check_connection();
	EVENT_SUBMIT(ev);
	ztest_returns_value(stg_config_u8_read, 0);
	zassert_false(event_manager_init(), "Error when initializing event manager");
	ztest_returns_value(reset_modem, 0);
	ztest_returns_value(lte_init, 0);
	ztest_returns_value(stg_config_str_read, 0);
	ztest_returns_value(check_ip, 0);
	ztest_returns_value(socket_connect, 0);
	int8_t err = cellular_controller_init();
	zassert_equal(err, 0, "Cellular controller initialization incomplete!");
	reset_test_semaphores();
}

static char dummy_test_msg[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
int received;
void test_socket_send_fails1(void)
{
	ztest_returns_value(check_ip, 0);
	ztest_returns_value(send_tcp, -1);
	ztest_returns_value(stop_tcp, -1); /* this will reset modem_is_ready for next test */

	struct check_connection *ev = new_check_connection();
	EVENT_SUBMIT(ev);
	struct messaging_proto_out_event *test_msgOut = new_messaging_proto_out_event();
	test_msgOut->buf = &dummy_test_msg[0];
	test_msgOut->len = sizeof(dummy_test_msg);
	EVENT_SUBMIT(test_msgOut);

	/* in case of failure, the sending thread will publish a
	 * stop_connection_event*/
	int err = k_sem_take(&stop_connection_sem, K_MSEC(500));
	zassert_equal(err, 0, "Expected stop connection event was not published ");

	reset_test_semaphores();
}

void test_socket_send_recovery1(void)
{
	ztest_returns_value(reset_modem, 0);
	ztest_returns_value(lte_init, 0);
	ztest_returns_value(stg_config_str_read, 0);
	ztest_returns_value(check_ip, 0);
	ztest_returns_value(socket_connect, 0);

	ztest_returns_value(send_tcp, sizeof(dummy_test_msg));

	struct check_connection *ev = new_check_connection();
	EVENT_SUBMIT(ev);

	struct messaging_proto_out_event *test_msgOut = new_messaging_proto_out_event();
	test_msgOut->buf = &dummy_test_msg[0];
	test_msgOut->len = sizeof(dummy_test_msg);
	EVENT_SUBMIT(test_msgOut);

	int err = k_sem_take(&cellular_ack, K_MSEC(100));
	zassert_equal(err, 0, "Expected cellular_ack event was not published ");

	err = k_sem_take(&send_out_sem, K_MSEC(500));
	zassert_equal(err, 0, "Expected free memory event was not published ");

	reset_test_semaphores();
}

void test_socket_send_fails2(void)
{
	ztest_returns_value(check_ip, 0);
	ztest_returns_value(send_tcp, -1);
	ztest_returns_value(stop_tcp, 0);

	struct check_connection *ev = new_check_connection();
	EVENT_SUBMIT(ev);
	struct messaging_proto_out_event *test_msgOut = new_messaging_proto_out_event();
	test_msgOut->buf = &dummy_test_msg[0];
	test_msgOut->len = sizeof(dummy_test_msg);
	EVENT_SUBMIT(test_msgOut);

	/* in case of failure, the sending thread will publish a
	 * stop_connection_event*/
	int err = k_sem_take(&stop_connection_sem, K_MSEC(500));
	zassert_equal(err, 0, "Expected stop connection event was not published ");

	reset_test_semaphores();
}

void test_socket_send_recovery2(void)
{
	ztest_returns_value(check_ip, 0);
	ztest_returns_value(socket_connect, 0);

	ztest_returns_value(send_tcp, sizeof(dummy_test_msg));

	struct check_connection *ev = new_check_connection();
	EVENT_SUBMIT(ev);

	struct messaging_proto_out_event *test_msgOut = new_messaging_proto_out_event();
	test_msgOut->buf = &dummy_test_msg[0];
	test_msgOut->len = sizeof(dummy_test_msg);
	EVENT_SUBMIT(test_msgOut);

	int err = k_sem_take(&cellular_ack, K_MSEC(100));
	zassert_equal(err, 0, "Expected cellular_ack event was not published ");

	err = k_sem_take(&send_out_sem, K_MSEC(500));
	zassert_equal(err, 0, "Expected free memory event was not published ");

	reset_test_semaphores();
}

void test_send_many_messages(void)
{
	char test_msg[100];
	size_t size2check = 0;
	for (int i = 0; i < 30; i++) {
		size2check = 0;
		for (int j = 0; j <= i; j++) {
			test_msg[j] = (char)(i + j);
			size2check += 1;
		}
		ztest_returns_value(check_ip, 0);
		ztest_returns_value(send_tcp, size2check);
		printk("%d\n", (int)size2check);

		struct check_connection *ev = new_check_connection();
		EVENT_SUBMIT(ev);

		struct messaging_proto_out_event *test_msgOut = new_messaging_proto_out_event();
		test_msgOut->buf = &test_msg[0];
		test_msgOut->len = size2check;
		EVENT_SUBMIT(test_msgOut);

		int err = k_sem_take(&cellular_ack, K_MSEC(150));
		zassert_equal(err, 0, "Expected cellular_ack event was not published ");

		err = k_sem_take(&send_out_sem, K_MSEC(500));
		zassert_equal(err, 0, "Expected free memory event was not published ");

		k_sleep(K_MSEC(10));
	}

	/*TODO: must refactor some source files to be able to test arguments
	 * passed to send_tcp function and the free-up message memory event.*/
	//	err = k_sem_take(&send_out_sem, K_MSEC(500));
	//	zassert_equal(err, 0,
	//		      "Expected free message memory event was not"
	//		      " published ");
	reset_test_semaphores();
}
int socket_poll_interval = 1000; /* must be set equal to SOCKET_POLL_INTERVAL */
void test_publish_event_with_a_received_msg(void) /* happy scenario - msg
 * received from server is pushed to messaging module! */
{
	received = 30;
	ztest_returns_value(socket_receive, received);
	int err = k_sem_take(&cellular_proto_in, K_SECONDS(1));
	zassert_equal(err, 0,
		      "Expected cellular_proto_in event was not "
		      "published ");

	struct messaging_ack_event *ack = new_messaging_ack_event();
	EVENT_SUBMIT(ack);

	received = 10;
	ztest_returns_value(socket_receive, received);

	k_sleep(K_MSEC(socket_poll_interval));
	err = k_sem_take(&cellular_proto_in, K_MSEC(50));
	zassert_equal(err, 0,
		      "Expected cellular_proto_in event was not "
		      "published ");

	/* simulate messaging module not publishing it's ack- for the next test
	struct messaging_ack_event *ack = new_messaging_ack_event();
	EVENT_SUBMIT(ack);
	 */
	reset_test_semaphores();
}

void test_ack_from_messaging_module_missed(void)
{
	received = 30;
	ztest_returns_value(socket_receive, received);
	int err = k_sem_take(&cellular_error,
			     K_MSEC(socket_poll_interval + CONFIG_MESSAGING_ACK_TIMEOUT_MSEC + 50));
	zassert_equal(err, 0, "Expected cellular_error event was not published!");
	ztest_returns_value(socket_receive, 0);
	ztest_returns_value(socket_receive, 0);

	k_sleep(K_MSEC(socket_poll_interval));

	err = k_sem_take(&cellular_proto_in, K_MSEC(socket_poll_interval));
	zassert_not_equal(err, 0, "Unexpected cellular_proto_in event was published!");
	reset_test_semaphores();
}

void test_socket_rcv_fails(void)
{
	ztest_returns_value(socket_receive, -1);
	ztest_returns_value(stop_tcp, 0);
	k_sleep(K_MSEC(socket_poll_interval));

	int err;
	err = k_sem_take(&cellular_error, K_MSEC(socket_poll_interval));
	zassert_equal(err, 0,
		      "Expected cellular_error event was not published on socket receive error!");
	reset_test_semaphores();
}

void test_socket_connect_fails(void)
{
	ztest_returns_value(check_ip, 0);
	ztest_returns_value(socket_connect, -1);
	ztest_returns_value(stop_tcp, 0);
	int err;
	struct check_connection *ev = new_check_connection();
	EVENT_SUBMIT(ev);
	err = k_sem_take(&cellular_error, K_SECONDS(1));
	zassert_equal(err, 0,
		      "Expected cellular_error event was not published on socket connect error!");
	reset_test_semaphores();
}

void test_notify_messaging_module_when_nudged_from_server(void)
{
	k_sem_give(&listen_sem); /*TODO: when unit testing the modem driver,
 * make sure the semaphore is given as expected.*/
	int err = k_sem_take(&wake_up, K_MSEC(100));
	zassert_equal(err, 0, "Failed to notify messaging module when nudged from server!");
	reset_test_semaphores();
}

void test_socket_rcv_times_out(void)
{
	ztest_returns_value(reset_modem, 0);
	ztest_returns_value(lte_init, 0);
	ztest_returns_value(stg_config_str_read, 0);
	ztest_returns_value(check_ip, 0);
	ztest_returns_value(socket_connect, 0);

	ztest_returns_value(send_tcp, sizeof(dummy_test_msg));

	struct check_connection *ev = new_check_connection();
	EVENT_SUBMIT(ev);

	struct messaging_proto_out_event *test_msgOut = new_messaging_proto_out_event();
	test_msgOut->buf = &dummy_test_msg[0];
	test_msgOut->len = sizeof(dummy_test_msg);
	EVENT_SUBMIT(test_msgOut);

	int err = k_sem_take(&cellular_ack, K_MSEC(100));
	zassert_equal(err, 0, "Expected cellular_ack event was not published ");

	err = k_sem_take(&send_out_sem, K_MSEC(500));
	zassert_equal(err, 0, "Expected free memory event was not published ");

	int cycles = 10; /* must be set to SOCK_RECV_TIMEOUT */
	while (cycles-- > 0) {
		ztest_returns_value(socket_receive, 0);
		k_sleep(K_MSEC(socket_poll_interval));
	}
	ztest_returns_value(socket_receive, 0);
	ztest_returns_value(stop_tcp, 0);
	k_sleep(K_MSEC(socket_poll_interval));
	reset_test_semaphores();
}

void test_recover_when_modem_wakeup_fails(void)
{
	k_sleep(K_SECONDS(30));
	test_socket_connect_fails();
	ztest_returns_value(reset_modem, -1);
	ztest_returns_value(modem_nf_pwr_off, 0);
	struct check_connection *ev = new_check_connection();
	EVENT_SUBMIT(ev); /* trigger failing case */

	ztest_returns_value(reset_modem, 0);
	ztest_returns_value(lte_init, 0);
	ztest_returns_value(stg_config_str_read, 0);
	ztest_returns_value(check_ip, 0);
	ztest_returns_value(socket_connect, 0);

	ztest_returns_value(send_tcp, sizeof(dummy_test_msg));
	struct messaging_proto_out_event *test_msgOut = new_messaging_proto_out_event();
	test_msgOut->buf = &dummy_test_msg[0];
	test_msgOut->len = sizeof(dummy_test_msg);
	EVENT_SUBMIT(test_msgOut);

	int err = k_sem_take(&cellular_ack, K_MSEC(100));
	zassert_equal(err, 0, "Expected cellular_ack event was not published ");

	err = k_sem_take(&send_out_sem, K_MSEC(500));
	zassert_equal(err, 0, "Expected free memory event was not published ");

	struct check_connection *ev2 = new_check_connection();
	EVENT_SUBMIT(ev2); /* trigger recovery */

	int cycles = 10; /* must be set to SOCK_RECV_TIMEOUT */
	while (cycles-- > 0) {
		ztest_returns_value(socket_receive, 0);
		k_sleep(K_MSEC(socket_poll_interval));
	}
	ztest_returns_value(socket_receive, 0);
	ztest_returns_value(stop_tcp, 0);
	k_sleep(K_MSEC(socket_poll_interval));
	reset_test_semaphores();
}

void test_modem_pwr_off(void)
{
	struct pwr_status_event *pwr_event = new_pwr_status_event();
	pwr_event->pwr_state = PWR_LOW;
	pwr_event->battery_mv = 330;
	pwr_event->battery_mv_min = 330;
	pwr_event->battery_mv_max = 330;
	EVENT_SUBMIT(pwr_event);

	reset_test_semaphores();

	struct check_connection *ev = new_check_connection();
	EVENT_SUBMIT(ev);

	ztest_returns_value(modem_nf_pwr_off, 0);

	k_sleep(K_MSEC(socket_poll_interval));
	struct pwr_status_event *pwr_event2 = new_pwr_status_event();
	pwr_event2->pwr_state = PWR_NORMAL;
	pwr_event2->battery_mv = 330;
	pwr_event2->battery_mv_min = 330;
	pwr_event2->battery_mv_max = 330;
	EVENT_SUBMIT(pwr_event2);

	reset_test_semaphores();

	test_socket_send_recovery1();
}

void test_gsm_device_not_ready(void)
{
	simulate_modem_down = true;

	int8_t err = cellular_controller_init();
	zassert_not_equal(err, 0, "Cellular controller initialization incomplete!");
	err = k_sem_take(&cellular_error, K_SECONDS(1));
	zassert_equal(err, 0, "Expected cellular_error event was not published on modem down!");
	reset_test_semaphores();
}

void test_download_new_modem_firmware_OK_installation_OK(void)
{
	int len = 22;
	char mdm_fw_url[] = "mdm_firmware_file";
	ztest_expect_value(modem_nf_ftp_fw_download, filename, &mdm_fw_url[0]);
	ztest_returns_value(modem_nf_ftp_fw_download, 0);
	ztest_returns_value(stop_tcp, 0);

	int err = k_sem_take(&connection_ready_sem, K_MSEC(100));
	zassert_equal(err, 0, "Unexpected connection state!");

	struct check_connection *ev = new_check_connection();
	EVENT_SUBMIT(ev);

	struct messaging_mdm_fw_event *mdm_fw_ver = new_messaging_mdm_fw_event();
	mdm_fw_ver->buf = &mdm_fw_url[0];
	mdm_fw_ver->len = len;
	EVENT_SUBMIT(mdm_fw_ver);

	err = k_sem_take(&download_complete_sem, K_MSEC(100));
	zassert_equal(err, 0, "Expected download complete event not published!%d", err);

	ztest_returns_value(modem_nf_ftp_fw_install, 0);
	ztest_returns_value(reset_modem, 0);
	ztest_returns_value(lte_init, 0);
	ztest_returns_value(stg_config_str_read, 0);
	ztest_returns_value(check_ip, 0);
	ztest_returns_value(socket_connect, 0);

	struct check_connection *ev2 = new_check_connection();
	EVENT_SUBMIT(ev2);

	struct messaging_mdm_fw_event *mdm_fw_ver2 = new_messaging_mdm_fw_event();
	mdm_fw_ver2->buf = "\0";
	EVENT_SUBMIT(mdm_fw_ver2);

	err = k_sem_take(&connection_ready_sem, K_MSEC(100));
	zassert_equal(err, 0, "Unexpected connection state!");
}

void test_main(void)
{
	int mdm_fw_iteration = 0;
	printk("%d\n", mdm_fw_iteration);
	ztest_test_suite(cellular_controller_tests1, ztest_unit_test(test_init),
			 ztest_unit_test(test_socket_send_fails1),
			 ztest_unit_test(test_socket_send_recovery1),
			 ztest_unit_test(test_socket_send_fails2),
			 ztest_unit_test(test_socket_send_recovery2),
			 ztest_unit_test(test_send_many_messages),
			 ztest_unit_test(test_publish_event_with_a_received_msg),
			 ztest_unit_test(test_ack_from_messaging_module_missed),
			 ztest_unit_test(test_socket_rcv_fails),
			 ztest_unit_test(test_socket_connect_fails),
			 ztest_unit_test(test_notify_messaging_module_when_nudged_from_server),
			 ztest_unit_test(test_socket_rcv_times_out),
			 ztest_unit_test(test_recover_when_modem_wakeup_fails),
			 ztest_unit_test(test_modem_pwr_off),
			 ztest_unit_test(test_gsm_device_not_ready),
			 ztest_unit_test(test_download_new_modem_firmware_OK_installation_OK));

	ztest_run_test_suite(cellular_controller_tests1);
}

static bool event_handler(const struct event_header *eh)
{
	if (is_cellular_proto_in_event(eh)) {
		k_sem_give(&cellular_proto_in);
		printk("released semaphore for cellular_proto_in!\n");
		struct cellular_proto_in_event *ev = cast_cellular_proto_in_event(eh);
		if (received > 0) {
			zassert_equal(ev->len, received, "Buffer length mismatch");
		}
		return false;
	} else if (is_cellular_ack_event(eh)) {
		k_sem_give(&cellular_ack);
		printk("released semaphore for cellular_ack, ");
		struct cellular_ack_event *ev = cast_cellular_ack_event(eh);
		if (ev->message_sent) {
			printk("sent successfully!\n");
		} else {
			printk("sending failed!\n");
		}
		return false;
	} else if (is_error_event(eh)) {
		k_sem_give(&cellular_error);
		printk("released semaphore for cellular_error!\n");
		return false;
	} else if (is_send_poll_request_now(eh)) {
		k_sem_give(&wake_up);
		printk("released semaphore for nudge from server!\n");
		return false;
	} else if (is_free_message_mem_event(eh)) {
		k_sem_give(&send_out_sem);
		return false;
	} else if (is_messaging_stop_connection_event(eh)) {
		k_sem_give(&stop_connection_sem);
		return false;
	} else if (is_mdm_fw_update_event(eh)) {
		struct mdm_fw_update_event *ev = cast_mdm_fw_update_event(eh);
		if (ev->status == MDM_FW_DOWNLOAD_COMPLETE) {
			printk("Download complete.\n");
			k_sem_give(&download_complete_sem);
		}
		return false;
	} else if (is_connection_state_event(eh)) {
		struct connection_state_event *ev = cast_connection_state_event(eh);
		if (ev->state) {
			k_sem_give(&connection_ready_sem);
		} else {
			k_sem_give(&connection_not_ready_sem);
		}
		return false;
	}
	return false;
}

EVENT_LISTENER(test, event_handler);
EVENT_SUBSCRIBE(test, cellular_proto_in_event);
EVENT_SUBSCRIBE(test, cellular_ack_event);
EVENT_SUBSCRIBE(test, error_event);
EVENT_SUBSCRIBE(test, send_poll_request_now);
EVENT_SUBSCRIBE(test, free_message_mem_event);
EVENT_SUBSCRIBE(test, messaging_stop_connection_event);
EVENT_SUBSCRIBE(test, connection_state_event);
EVENT_SUBSCRIBE(test, mdm_fw_update_event);
