#include <ztest.h>
#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <drivers/gpio/gpio_emul.h>
#include <net/net_if.h>
#include <net/socket.h>
#include <modem_nf.h>
#include <modem_context.h>
#include "uart_mock.h"

/* TODO, refactor the forked modem driver so we don't need externals */

#define MODEM_VERBOSE

struct k_sem listen_sem;

static K_MUTEX_DEFINE(at_mutex);

/* Holds information of expected commands and responses */
#define MODEM_CMD_RESP_MAX 200

typedef struct modem_cmd_resp_t {
	char *cmd;
	char *rsp;
	k_timeout_t response_delay;

} modem_cmd_resp_t;

static modem_cmd_resp_t cmd_resp[MODEM_CMD_RESP_MAX];
static uint32_t cmd_resp_cnt = 0;
uint32_t cmd_resp_ind = 0;

/* Signals that CMD/RESP has completed successfully */
static struct k_sem modem_sem;

const struct device *uart_dev = DEVICE_DT_GET(DT_ALIAS(modemuart));
const struct device *modem_dev = DEVICE_DT_GET(DT_ALIAS(modem));
const struct device *gpio0_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));

#define MDM_VINT_PIN DT_GPIO_PIN(DT_NODELABEL(modem), mdm_vint_gpios)

/* Parser thread structures */
K_KERNEL_STACK_DEFINE(modem_dev_stack, 2048);
struct k_thread modem_dev_thread;

/* Signals UART data incoming */
static struct k_sem modem_rx_sem;

/* Re-usable socket ID */
static int socket_id;

static void modem_clear_cmd_rsp()
{
	k_mutex_lock(&at_mutex, K_FOREVER);
	k_sem_reset(&modem_rx_sem);
	memset(&cmd_resp, 0, sizeof(cmd_resp));
	cmd_resp_cnt = 0;
	cmd_resp_ind = 0;
	k_mutex_unlock(&at_mutex);
}
/**
 * @brief Adds expected command&response in buffer
 *
 * @param[in] cmd Expected AT command string
 * @param[in] rsp AT response to send on receiving command
 * @param[in] delay, delay the thread should hang before replying
 *
 * @return 0 if everything was ok, error code otherwise
 */
static int modem_add_expected_cmd_rsp_delay(const char *cmd, const char *rsp, k_timeout_t delay)
{
	k_mutex_lock(&at_mutex, K_FOREVER);
	int ret = -ENOBUFS;
	size_t cmd_size = strlen(cmd) + 1;
	size_t rsp_size = strlen(rsp) + 1;
	if (cmd_resp_cnt < MODEM_CMD_RESP_MAX) {
		cmd_resp[cmd_resp_cnt].response_delay = delay;
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
	} else {
		ztest_test_fail();
	}
	k_mutex_unlock(&at_mutex);
	return ret;
}

/**
 * @brief Check data for expected cmd with no delay
 *
 * @param[in] data Data buffer holding data
 * @param[in] size Size of data in buffer
 *
 * @return 0 if everything was ok, error code otherwise
 */
static int modem_add_expected_cmd_rsp(const char *cmd, const char *rsp)
{
	return modem_add_expected_cmd_rsp_delay(cmd, rsp, K_NO_WAIT);
}

static int modem_process_cmd_resp(uint8_t *data, uint32_t size)
{
	if (cmd_resp[cmd_resp_ind].cmd && size == strlen(cmd_resp[cmd_resp_ind].cmd)) {
#ifdef MODEM_VERBOSE
		printk("Parsed DATA: %.*s\n", size, data);
#endif
		if (strncmp(cmd_resp[cmd_resp_ind].cmd, data, size) == 0) {
#ifdef MODEM_VERBOSE

			printk("\nMATCH! %s returns %s\n", cmd_resp[cmd_resp_ind].cmd,
			       cmd_resp[cmd_resp_ind].rsp);
#endif
			if (cmd_resp[cmd_resp_ind].rsp) {
				k_sleep(cmd_resp[cmd_resp_ind].response_delay);
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
				k_sleep(cmd_resp[cmd_resp_ind].response_delay);
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
		k_mutex_lock(&at_mutex, K_FOREVER);
		if (k_sem_take(&modem_rx_sem, K_MSEC(10)) == 0) {
			unit_test_timeout_count = 0;
			mock_uart_receive(uart_dev, &data[total_size], &size, sizeof(data), true);
			total_size += size;
			if (total_size > 0) {
				if (modem_process_cmd_resp(data, total_size) == 0) {
					total_size = 0;
				}
			}

		} else {
			unit_test_timeout_count++;
			modem_process_empty_cmd_resp();
		}
		k_mutex_unlock(&at_mutex);
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

/**
 * Plays a modem reset function. I.e. emulate AT response to
 * all expected AT-commands sent from the SARA-R4 modem reset function
 * @note: If you change any commands in the modem, this function will break.
 * @param work the k_work
 */
static void play_modem_reset_at_response_fnc(struct k_work *work)
{
	/* Pretend modem vint goes high */
	gpio_emul_input_set(gpio0_dev, MDM_VINT_PIN, 1);
	k_sleep(K_SECONDS(1));
	/* Pin 26 is rx-pin */
	gpio_emul_input_set(gpio0_dev, 26, 1);
	modem_add_expected_cmd_rsp("AT\r", "OK\r");
	modem_add_expected_cmd_rsp("ATE0\r", "OK\r");
	modem_add_expected_cmd_rsp("AT+UMNOPROF=100\r", "OK\r");
	modem_add_expected_cmd_rsp("AT+CFUN=15\r", "OK\r");
	modem_add_expected_cmd_rsp("AT\r", "OK\r");
	modem_add_expected_cmd_rsp("ATE0\r", "OK\r");
	modem_add_expected_cmd_rsp("AT+COPS=2\r", "OK\r");
	modem_add_expected_cmd_rsp("AT+URAT=7,9\r", "OK\r");
	modem_add_expected_cmd_rsp("AT+CPSMS=0\r", "OK\r");
	/* modem_add_expected_cmd_rsp("AT+UBANDMASK=0,526494,2\r", "OK\r"); */
	modem_add_expected_cmd_rsp("AT+CFUN=15\r", "OK\r");
	modem_add_expected_cmd_rsp("AT\r", "OK\r");
	/* @todo: bug in driverc?, why 2 ATE0 necessary here ? */
	modem_add_expected_cmd_rsp("ATE0\r", "OK\r");
	modem_add_expected_cmd_rsp("ATE0\r", "OK\r");
	modem_add_expected_cmd_rsp("AT+CRSM=214,28531,0,0,14,"
				   "\"FFFFFFFFFFFFFFFFFFFFFFFFFFFF\"\r",
				   "OK\r");
	modem_add_expected_cmd_rsp("AT+CRSM=214, 28539, 0, 0, 12,"
				   "\"FFFFFFFFFFFFFFFFFFFFFFFF\"\r",
				   "OK\r");
	modem_add_expected_cmd_rsp("AT+CFUN=0\r", "OK\r");
	modem_add_expected_cmd_rsp("AT+CMEE=1\r", "OK\r");
	modem_add_expected_cmd_rsp("AT+CREG=1\r", "OK\r");

	modem_add_expected_cmd_rsp("AT+CGMI\r", "u-blox\r"
						"OK\r");
	modem_add_expected_cmd_rsp("AT+CGMM\r", "SARA-R422S\r"
						"OK\r");
	modem_add_expected_cmd_rsp("AT+CGMR\r", "00.12\r"
						"OK\r");
	modem_add_expected_cmd_rsp("ATI0\r", "SARA-R422S-00B-00\r"
					     "OK\r");
	modem_add_expected_cmd_rsp("AT+CGSN\r", "355439110049898\r"
						"OK\r");
	modem_add_expected_cmd_rsp("AT+CIMI\r", "240075815498371\r"
						"OK\r");
	modem_add_expected_cmd_rsp("AT+CCID\r", "89462038007006878224\r"
						"OK\r");
	modem_add_expected_cmd_rsp("AT+CGDCONT=1,\"IP\",\"hologram\"\r", "OK\r");
	modem_add_expected_cmd_rsp("AT+CFUN=1\r", "OK\r");
	modem_add_expected_cmd_rsp("ATE0\r", "OK\r");
	modem_add_expected_cmd_rsp("AT+UPSV=0\r", "OK\r");
	modem_add_expected_cmd_rsp("AT+UPSV?\r", "+UPSV: 0\r"
						 "OK\r");
	modem_add_expected_cmd_rsp("AT+COPS=0,2\r", "OK\r");
	modem_add_expected_cmd_rsp("AT+CFUN=0\r", "OK\r");
	modem_add_expected_cmd_rsp("AT+CFUN=1\r", "OK\r"
						  "+CREG: 1\r");
	/* @todo why two ATE0 ? */
	modem_add_expected_cmd_rsp("ATE0\r", "OK\r");
	modem_add_expected_cmd_rsp("ATE0\r", "OK\r");
	modem_add_expected_cmd_rsp("AT+CSQ\r", "+CSQ: 25,99\r"
					       "OK\r");
	modem_add_expected_cmd_rsp("AT+URAT?\r", "+URAT: 7 ,9\r"
						 "OK\r");
	modem_add_expected_cmd_rsp("AT+COPS?\r", "+COPS: 0 ,2,\"24202\",3\r"
						 "OK\r");
	modem_add_expected_cmd_rsp("AT+UPSD=0,0,0\r", "OK\r");
	modem_add_expected_cmd_rsp("AT+UPSD=0,100,1\r", "OK\r");
	modem_add_expected_cmd_rsp("AT+CGACT=1,1\r", "OK\r");
	modem_add_expected_cmd_rsp("AT+CGACT?\r", "+CGACT: 1,1\r"
						  "OK\r");
}

static void play_modem_reset_in_workqueue()
{
	static struct k_work_delayable play_delayable;
	k_work_init_delayable(&play_delayable, play_modem_reset_at_response_fnc);
	k_work_reschedule(&play_delayable, K_SECONDS(50));
}

static int modem_read_status_nvm(uint8_t *status)
{
	*status = 0xFF;
	return 0;
}

static int modem_write_status_nvm(uint8_t status)
{
	return 0;
}

static void test_modem_nf_get_model_and_fw_version()
{
	modem_clear_cmd_rsp();
	set_modem_status_cb(modem_read_status_nvm, modem_write_status_nvm);
	const char *model;
	const char *version;
	/* Be sure to clean internal modem data structure */
	struct modem_context *ctx = modem_context_from_id(0);
	ctx->data_model[0] = '\0';
	ctx->data_revision[0] = '\0';
	/* play the modem reset */
	play_modem_reset_in_workqueue();
	/* check values */
	int ret = modem_nf_get_model_and_fw_version(&model, &version);
	zassert_equal(ret, -ENODATA, "");
	ret = modem_nf_reset();
	zassert_equal(ret, 0, "");
	ret = modem_nf_get_model_and_fw_version(&model, &version);
	zassert_equal(ret, 0, "");
	zassert_true(strcmp(model, "SARA-R422S-00B-00") == 0, model);
	zassert_true(strcmp(version, "00.12") == 0, version);
	zassert_equal(k_sem_take(&modem_sem, K_MSEC(1000)), 0, "");
}

/**
 * @brief Test the happy, normal operation of the FTP download
 */
static void test_modem_nf_ftp_fw_download_success()
{
	struct modem_nf_uftp_params params = { .ftp_server = "999.888.777.666",
					       .ftp_user = "ftp_user",
					       .ftp_password = "ftp_password",
					       .download_timeout_sec = 0 };
	modem_clear_cmd_rsp();
	modem_add_expected_cmd_rsp("AT+UFTP=0,\"999.888.777.666\"\r", "OK\r");
	modem_add_expected_cmd_rsp("AT+UFTP=2,\"ftp_user\"\r", "OK\r");
	modem_add_expected_cmd_rsp("AT+UFTP=3,\"ftp_password\"\r", "OK\r");

	modem_add_expected_cmd_rsp("AT+UFTPC=1\r", "OK\r");
	modem_add_expected_cmd_rsp_delay("", "+UUFTPCR: 1,1\r", K_SECONDS(59));

	modem_add_expected_cmd_rsp("AT+UFTPC=100,\"test_file_123.udf\"\r", "OK\r");
	modem_add_expected_cmd_rsp_delay(
		"", "+UUFTPCR: 100,1,\"91aac9bf3c59d06cf22d92d40a0ee84c\"\r", K_SECONDS(3600 - 1));

	int ret = modem_nf_ftp_fw_download(&params, "test_file_123.udf");
	zassert_equal(ret, 0, "");
	zassert_equal(k_sem_take(&modem_sem, K_MSEC(1000)), 0, "");
}

/**
 * @brief test NULL parameters
 */
static void test_modem_nf_ftp_fw_download_null_params()
{
	struct modem_nf_uftp_params params = { .ftp_server = "999.888.777.666",
					       .ftp_user = "ftp_user",
					       .ftp_password = "ftp_password",
					       .download_timeout_sec = 0 };
	modem_clear_cmd_rsp();
	int ret = modem_nf_ftp_fw_download(NULL, "test_file_123.udf");
	zassert_equal(ret, -EINVAL, "");
	ret = modem_nf_ftp_fw_download(&params, NULL);
	zassert_equal(ret, -EINVAL, "");
}

/**
 * @brief test that AT setup commands fail, e.g. due to dead modem
 */
static void test_modem_nf_ftp_fw_download_setup_fails()
{
	struct modem_nf_uftp_params params = { .ftp_server = "999.888.777.666",
					       .ftp_user = "ftp_user",
					       .ftp_password = "ftp_password",
					       .download_timeout_sec = 3600 };
	modem_clear_cmd_rsp();
	modem_add_expected_cmd_rsp("AT+UFTP=0,\"999.888.777.666\"\r", "ERROR\r");
	int ret = modem_nf_ftp_fw_download(&params, "test_file_123.udf");
	zassert_equal(ret, -EIO, "");
	zassert_equal(k_sem_take(&modem_sem, K_MSEC(1000)), 0, "");
}

/**
 * @brief Test the FTP download when we timeout on a "connect"
 */
static void test_modem_nf_ftp_fw_download_timeout_connect()
{
	struct modem_nf_uftp_params params = { .ftp_server = "999.888.777.666",
					       .ftp_user = "ftp_user",
					       .ftp_password = "ftp_password",
					       .download_timeout_sec = 3600 };
	modem_clear_cmd_rsp();
	modem_add_expected_cmd_rsp("AT+UFTP=0,\"999.888.777.666\"\r", "OK\r");
	modem_add_expected_cmd_rsp("AT+UFTP=2,\"ftp_user\"\r", "OK\r");
	modem_add_expected_cmd_rsp("AT+UFTP=3,\"ftp_password\"\r", "OK\r");

	modem_add_expected_cmd_rsp("AT+UFTPC=1\r", "OK\r");
	modem_add_expected_cmd_rsp_delay("", "+UUFTPCR: 1,1\r", K_SECONDS(61));

	int ret = modem_nf_ftp_fw_download(&params, "test_file_123.udf");
	zassert_equal(ret, -ETIMEDOUT, "");
	/* Assert that the last URC was delivered too late to read */
	k_sem_take(&modem_sem, K_MSEC(1000));
	zassert_equal(cmd_resp_ind, cmd_resp_cnt - 1, "");
}

/**
 * @brief Test the FTP download when the connection is refused
 */
static void test_modem_nf_ftp_fw_download_connect_refused()
{
	struct modem_nf_uftp_params params = { .ftp_server = "999.888.777.666",
					       .ftp_user = "ftp_user",
					       .ftp_password = "ftp_password",
					       .download_timeout_sec = 3600 };
	modem_clear_cmd_rsp();
	modem_add_expected_cmd_rsp("AT+UFTP=0,\"999.888.777.666\"\r", "OK\r");
	modem_add_expected_cmd_rsp("AT+UFTP=2,\"ftp_user\"\r", "OK\r");
	modem_add_expected_cmd_rsp("AT+UFTP=3,\"ftp_password\"\r", "OK\r");

	modem_add_expected_cmd_rsp("AT+UFTPC=1\r", "OK\r");
	modem_add_expected_cmd_rsp_delay("", "+UUFTPCR: 1,0\r", K_SECONDS(1));

	int ret = modem_nf_ftp_fw_download(&params, "test_file_123.udf");
	zassert_equal(ret, -ECONNREFUSED, "");
	k_sem_take(&modem_sem, K_MSEC(1000));
}

/**
 * @brief Test the FTP download when we timeout on the file tranfer
 */
static void test_modem_nf_ftp_fw_download_timeout_transfer()
{
	struct modem_nf_uftp_params params = { .ftp_server = "999.888.777.666",
					       .ftp_user = "ftp_user",
					       .ftp_password = "ftp_password",
					       .download_timeout_sec = 3600 };
	modem_clear_cmd_rsp();

	modem_add_expected_cmd_rsp("AT+UFTP=0,\"999.888.777.666\"\r", "OK\r");
	modem_add_expected_cmd_rsp("AT+UFTP=2,\"ftp_user\"\r", "OK\r");
	modem_add_expected_cmd_rsp("AT+UFTP=3,\"ftp_password\"\r", "OK\r");

	modem_add_expected_cmd_rsp("AT+UFTPC=1\r", "OK\r");
	modem_add_expected_cmd_rsp_delay("", "+UUFTPCR: 1,1\r", K_SECONDS(59));

	modem_add_expected_cmd_rsp("AT+UFTPC=100,\"test_file_123.udf\"\r", "OK\r");
	modem_add_expected_cmd_rsp_delay(
		"", "+UUFTPCR: 100,1,\"91aac9bf3c59d06cf22d92d40a0ee84c\"\r", K_SECONDS(3600 + 1));

	int ret = modem_nf_ftp_fw_download(&params, "test_file_123.udf");
	zassert_equal(ret, -ETIMEDOUT, "");
	/* Assert that the last URC was delivered too late to read */
	k_sem_take(&modem_sem, K_MSEC(1000));
	zassert_equal(cmd_resp_ind, cmd_resp_cnt - 1, "");
}

/**
 * @brief Test the FTP download when the file transfer +UUFTPCR indicates error
 */
static void test_modem_nf_ftp_fw_download_error_transfer()
{
	struct modem_nf_uftp_params params = { .ftp_server = "999.888.777.666",
					       .ftp_user = "ftp_user",
					       .ftp_password = "ftp_password",
					       .download_timeout_sec = 3600 };
	modem_clear_cmd_rsp();

	modem_add_expected_cmd_rsp("AT+UFTP=0,\"999.888.777.666\"\r", "OK\r");
	modem_add_expected_cmd_rsp("AT+UFTP=2,\"ftp_user\"\r", "OK\r");
	modem_add_expected_cmd_rsp("AT+UFTP=3,\"ftp_password\"\r", "OK\r");

	modem_add_expected_cmd_rsp("AT+UFTPC=1\r", "OK\r");
	modem_add_expected_cmd_rsp_delay("", "+UUFTPCR: 1,1\r", K_SECONDS(59));

	modem_add_expected_cmd_rsp("AT+UFTPC=100,\"test_file_123.udf\"\r", "OK\r");
	modem_add_expected_cmd_rsp_delay("", "+UUFTPCR: 100,0\r", K_SECONDS(1));

	int ret = modem_nf_ftp_fw_download(&params, "test_file_123.udf");
	zassert_equal(ret, -EIO, "");
	k_sem_take(&modem_sem, K_MSEC(1000));
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
			 ztest_unit_test(test_socket_recv_missing_urc),
			 ztest_unit_test(test_modem_nf_get_model_and_fw_version),
			 ztest_unit_test(test_modem_nf_ftp_fw_download_success),
			 ztest_unit_test(test_modem_nf_ftp_fw_download_null_params),
			 ztest_unit_test(test_modem_nf_ftp_fw_download_setup_fails),
			 ztest_unit_test(test_modem_nf_ftp_fw_download_connect_refused),
			 ztest_unit_test(test_modem_nf_ftp_fw_download_error_transfer),
			 ztest_unit_test(test_modem_nf_ftp_fw_download_timeout_connect),
			 ztest_unit_test(test_modem_nf_ftp_fw_download_timeout_transfer)

	);
	ztest_run_test_suite(common);
}
