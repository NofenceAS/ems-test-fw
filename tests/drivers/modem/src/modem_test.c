#include <ztest.h>
#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <drivers/gpio/gpio_emul.h>
#include <net/net_if.h>
#include <net/socket.h>
//#include "modem_nf.h"
#include "uart_mock.h"

/* TODO, refactor the forked modem driver so we don't need externals */

#define MODEM_VERBOSE

struct k_sem listen_sem;

/* Holds information of expected commands and responses */
#define MODEM_CMD_RESP_MAX 20
typedef struct modem_cmd_resp_t {
	char *cmd;
	char *rsp;
} modem_cmd_resp_t;

static modem_cmd_resp_t cmd_resp[MODEM_CMD_RESP_MAX];
static uint32_t cmd_resp_cnt = 0;
uint32_t cmd_resp_ind = 0;

/* Signals that CMD/RESP has completed successfully */
static struct k_sem modem_sem;

const struct device *uart_dev = DEVICE_DT_GET(DT_ALIAS(modemuart));
const struct device *modem_dev = DEVICE_DT_GET(DT_ALIAS(modem));
const struct device *gpio0_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));

#define EXTINT_PIN DT_GPIO_PIN(DT_NODELABEL(modem), extint_gpios)

/* Parser thread structures */
K_KERNEL_STACK_DEFINE(modem_dev_stack, 2048);
struct k_thread modem_dev_thread;

/* Signals UART data incoming */
static struct k_sem modem_rx_sem;

/* Re-usable socket ID */
static int socket_id;

/**
 * @brief Adds expected command&response in buffer
 *
 * @param[in] cmd Expected AT command string
 * @param[in] rsp AT response to send on receiving command
 *
 * @return 0 if everything was ok, error code otherwise
 */

static int modem_add_expected_cmd_rsp(const char *cmd, const char *rsp)
{
	int ret = -ENOBUFS;
	size_t cmd_size = strlen(cmd) + 1;
	size_t rsp_size = strlen(rsp) + 1;
	if (cmd_resp_cnt < MODEM_CMD_RESP_MAX) {
		if (cmd != NULL) {
			cmd_resp[cmd_resp_cnt].cmd = k_malloc(cmd_size);
			strncpy(cmd_resp[cmd_resp_cnt].cmd, cmd, cmd_size);
		} else {
			cmd_resp[cmd_resp_cnt].cmd = NULL;
		}

		if (rsp != NULL) {
			cmd_resp[cmd_resp_cnt].rsp = k_malloc(rsp_size);
			strncpy(cmd_resp[cmd_resp_cnt].rsp, rsp, rsp_size);
		} else {
			cmd_resp[cmd_resp_cnt].rsp = NULL;
		}

		cmd_resp_cnt++;
		ret = 0;
	}
	return ret;
}

/**
 * @brief Check data for expected cmd
 *
 * @param[in] data Data buffer holding data
 * @param[in] size Size of data in buffer
 *
 * @return 0 if everything was ok, error code otherwise
 */
static int modem_process_cmd_resp(uint8_t *data, uint32_t size)
{
	if (cmd_resp[cmd_resp_ind].cmd && size == strlen(cmd_resp[cmd_resp_ind].cmd)) {
#ifdef MODEM_VERBOSE
		printk("Parsed DATA: %s\n", data);
#endif
		if (strncmp(cmd_resp[cmd_resp_ind].cmd, data, size) == 0) {
#ifdef MODEM_VERBOSE
			printk("MATCH! %s returns %s\n", cmd_resp[cmd_resp_ind].cmd,
			       cmd_resp[cmd_resp_ind].rsp);
#endif
			if (cmd_resp[cmd_resp_ind].rsp) {
				mock_uart_send(uart_dev, cmd_resp[cmd_resp_ind].rsp,
					       strlen(cmd_resp[cmd_resp_ind].rsp));
			}

			cmd_resp_ind++;

			if (cmd_resp_ind == cmd_resp_cnt) {
				k_sem_give(&modem_sem);
			}
			return 0;
		} else {
			/* FAILED */
#ifdef MODEM_VERBOSE
			printk("FAILED, expected:\r\n");
			for (int i = 0; i < size; i++) {
				printk("0x%02X ", cmd_resp[cmd_resp_ind].cmd[i]);
			}
			printk("\r\n");
#endif
			return -ECOMM;
		}
	}

	return -EAGAIN;
}

/**
 * @brief Send out responses/data that do not expect commands first
 *
 * @return 0 if everything was ok, error code otherwise
 */
static int modem_process_empty_cmd_resp(void)
{
	if (cmd_resp_ind < cmd_resp_cnt) {
		if (cmd_resp[cmd_resp_ind].cmd != NULL && strlen(cmd_resp[cmd_resp_ind].cmd) == 0) {
			if (cmd_resp[cmd_resp_ind].rsp != NULL &&
			    strlen(cmd_resp[cmd_resp_ind].rsp) > 0) {
				mock_uart_send(uart_dev, cmd_resp[cmd_resp_ind].rsp,
					       strlen(cmd_resp[cmd_resp_ind].rsp));
			}
			cmd_resp_ind++;
			if (cmd_resp_ind == cmd_resp_cnt) {
				k_sem_give(&modem_sem);
			}
		}
	}

	return 0;
}

/**
 * @brief MODEM device simulator thread, handles expected command&response
 *        buffer and communication
 *
 * @param[in] dev Unused
 *
 */
static void modem_dev_sim(void *dev)
{
	uint32_t size = 0;
	uint32_t total_size = 0;
	uint8_t data[100];
	int unit_test_timeout_count = 0;

	while (true) {
		if (k_sem_take(&modem_rx_sem, K_MSEC(10)) == 0) {
			unit_test_timeout_count = 0;
			mock_uart_receive(uart_dev, &data[total_size], &size, sizeof(data), true);
			total_size += size;
			if (total_size > 0 && modem_process_cmd_resp(data, total_size) == 0) {
				total_size = 0;
			}

		} else {
			unit_test_timeout_count++;
			modem_process_empty_cmd_resp();
		}
		zassert_true(unit_test_timeout_count < 10000, "Detected test hang");
	}
}

/**
 * Test a happy scenario socket create, mostly to verify that the UART emulator
 * is working as expected
 */
static void test_simple_socket_connect()
{
	int ret;
	socket_id = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	zassert_equal(socket_id, 0, "");

	struct sockaddr_in addr4;
	addr4.sin_family = AF_INET;
	addr4.sin_port = htons(1234);
	inet_pton(AF_INET, "192.168.1.1", &addr4.sin_addr);

	modem_add_expected_cmd_rsp("AT+USOCR=6\r", "+USOCR: 2\rOK\r");
	modem_add_expected_cmd_rsp("AT+USOSO=2,65535,128,1,3000\r", "OK\r");
	modem_add_expected_cmd_rsp("AT+USOCO=2,\"192.168.1.1\",1234\r", "OK\r");

	ret = connect(socket_id, (const struct sockaddr *)&addr4, sizeof(addr4));
	zassert_equal(ret, 0, "");
	zassert_equal(k_sem_take(&modem_sem, K_MSEC(1000)), 0, "");
}

/**
 * Test simple socket recv
 */
static void test_simple_socket_recv()
{
	int ret;
	char buf[100];
	memset(buf, 0, sizeof(buf));
	zassert_equal(socket_id, 0, "");
	modem_add_expected_cmd_rsp("", "+UUSORD:  2,10\r");
	modem_add_expected_cmd_rsp("AT+USORD=2,10\r", "+USORD:  2,10,\"0123456789\"\r"
						      "OK\r");
	ret = recv(socket_id, buf, sizeof(buf), 0);
	zassert_equal(ret, 10, "");
	zassert_mem_equal(buf, "0123456789", 10, "");
}

static void test_socket_recv_partial_read()
{
	int ret;
	char buf[10];
	memset(buf, 0, sizeof(buf));
	zassert_equal(socket_id, 0, "");
	modem_add_expected_cmd_rsp("", "+UUSORD:  2,10\r");

	modem_add_expected_cmd_rsp("AT+USORD=2,10\r", "+USORD:  2,10,\"0123");
	modem_add_expected_cmd_rsp("", "\r");
	modem_add_expected_cmd_rsp("", "456789\"\r"
				       "OK\r");

	ret = recv(socket_id, buf, sizeof(buf), 0);

	zassert_equal(ret, 10, "");
	zassert_mem_equal(buf, "0123456789", 10, buf);
}

/**
 * Test socket receive when the +UUSORD is missing.
 * @see XF-293
 */
static void test_socket_recv_missing_urc()
{
	int ret;
	char buf[10];
	memset(buf, 0, sizeof(buf));
	zassert_equal(socket_id, 0, "");
	/* Check timeout on recv */
	modem_add_expected_cmd_rsp("AT+USORD=2,10\r", "+USORD:  2,\"\"\r"
						      "OK\r");
	modem_add_expected_cmd_rsp("AT+USORD=2,10\r", "+UUSO\r"
						      "+USORD:  2,10,\"0123456789\"\r"
						      "OK\r");
	ret = recv(socket_id, buf, sizeof(buf), 0);

	zassert_equal(ret, 10, "");
	zassert_mem_equal(buf, "0123456789", 10, "");
}

void test_main(void)
{
	zassert_not_null(gpio0_dev, "GPIO is null");
	k_sem_init(&modem_rx_sem, 0, 1);
	mock_uart_register_sem(uart_dev, &modem_rx_sem);

	k_sem_init(&modem_sem, 0, 1);

	/* Start dev thread */
	k_thread_create(&modem_dev_thread, modem_dev_stack, K_KERNEL_STACK_SIZEOF(modem_dev_stack),
			(k_thread_entry_t)modem_dev_sim, (void *)modem_dev, NULL, NULL,
			K_PRIO_COOP(2), 0, K_NO_WAIT);

	ztest_test_suite(common, ztest_unit_test(test_simple_socket_connect),
			 ztest_unit_test(test_simple_socket_recv),
			 ztest_unit_test(test_socket_recv_missing_urc)

	);
	ztest_run_test_suite(common);
}
