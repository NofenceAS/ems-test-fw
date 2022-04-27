#include <ztest.h>
#include <zephyr.h>
#include <device.h>
#include "gnss.h"
#include "uart_mock.h"

const struct device *uart_dev = DEVICE_DT_GET(DT_ALIAS(gnssuart));
const struct device *gnss_dev = DEVICE_DT_GET(DT_ALIAS(gnss));

//#define GNSS_VERBOSE

/* Parser thread structures */
K_KERNEL_STACK_DEFINE(gnss_dev_stack,
		      2048);
struct k_thread gnss_dev_thread;

/* Signals UART data incoming */
static struct k_sem gnss_rx_sem;

/* Holds information of expected commands and responses */
#define GNSS_CMD_RESP_MAX 20
typedef struct gnss_cmd_resp_t {
	uint8_t* cmd;
	uint32_t cmd_size;
	uint8_t* rsp;
	uint32_t rsp_size;
} gnss_cmd_resp_t;
static gnss_cmd_resp_t cmd_resp[GNSS_CMD_RESP_MAX];
static uint32_t cmd_resp_cnt = 0;
uint32_t cmd_resp_ind = 0;

/* Signals that CMD/RESP has completed successfully */
static struct k_sem gnss_sem;

/* Signals that new data has been provided by GNSS callbacks */
static struct k_sem gnss_data_sem;

/* GNSS data structures */
static gnss_t gnss_cb_data;

/* GNSS data callback, copies data and signals via sem */
static int gnss_data_cb(const gnss_t* data)
{
#ifdef GNSS_VERBOSE
	printk("Got GNSS data!\r\n");
	printk("Lat=%d\r\n", data->lat);
	printk("Lon=%d\r\n", data->lon);
	printk("numSV=%d\r\n", data->num_sv);
	printk("hAcc=%d\r\n", data->h_acc_dm);
#endif
	memcpy(&gnss_cb_data, data, sizeof(gnss_t));

	k_sem_give(&gnss_data_sem);

	return 0;
}

/**
 * @brief Adds expected command&response in buffer 
 *
 * @param[in] cmd Expected command buffer
 * @param[in] cmd_size Expected command size
 * @param[in] rsp Response buffer to send on receiving command
 * @param[in] rsp_size Response size
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static int gnss_add_expected_cmd_rsp(const uint8_t* cmd, 
				     uint32_t cmd_size, 
				     const uint8_t* rsp,
				     uint32_t rsp_size)
{
	int ret = -ENOBUFS;
	if (cmd_resp_cnt < GNSS_CMD_RESP_MAX) {
		if (cmd != NULL) {
			cmd_resp[cmd_resp_cnt].cmd = k_malloc(cmd_size);
			cmd_resp[cmd_resp_cnt].cmd_size = cmd_size;
			memcpy(cmd_resp[cmd_resp_cnt].cmd,
				cmd, 
				cmd_size);
		} else {
			cmd_resp[cmd_resp_cnt].cmd = NULL;
			cmd_resp[cmd_resp_cnt].cmd_size = 0;
		}
		
		if (rsp != NULL) {
			cmd_resp[cmd_resp_cnt].rsp = k_malloc(rsp_size);
			cmd_resp[cmd_resp_cnt].rsp_size = rsp_size;
			memcpy(cmd_resp[cmd_resp_cnt].rsp,
				rsp, 
				rsp_size);
		} else {
			cmd_resp[cmd_resp_cnt].rsp = NULL;
			cmd_resp[cmd_resp_cnt].rsp_size = 0;
		}
		
		cmd_resp_cnt++;

		ret = 0;
	}
	return ret;
}

/**
 * @brief Remove all entries from expected command/response buffer
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static int gnss_clear_expected(void)
{
	cmd_resp_cnt = 0;
	cmd_resp_ind = 0;

	return 0;
}

/**
 * @brief Check data for expected cmd
 *
 * @param[in] data Data buffer holding data
 * @param[in] size Size of data in buffer
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static int gnss_process_cmd_resp(uint8_t* data, uint32_t size)
{
	if ((cmd_resp[cmd_resp_ind].cmd_size == 0) || 
		(size == cmd_resp[cmd_resp_ind].cmd_size)) {
#ifdef GNSS_VERBOSE
		printk("Parsed data: ");
		for (int i = 0; i < size; i++)
		{
			printk("0x%02X ", data[i]);
		}
		printk("\r\n");
#endif
		if ((cmd_resp[cmd_resp_ind].cmd_size == 0) || 
			(memcmp(cmd_resp[cmd_resp_ind].cmd, 
			        data, size) == 0)) {
#ifdef GNSS_VERBOSE
			printk("MATCH!\r\n");
#endif
			if (cmd_resp[cmd_resp_ind].rsp_size) {
				mock_uart_send(uart_dev, 
					    cmd_resp[cmd_resp_ind].rsp, 
					    cmd_resp[cmd_resp_ind].rsp_size);
			}

			cmd_resp_ind++;

			if (cmd_resp_ind == cmd_resp_cnt) {
				k_sem_give(&gnss_sem);
			}

			return 0;
		} else {
			/* FAILED */
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
static int gnss_process_empty_cmd_resp(void)
{
	if (cmd_resp_ind < cmd_resp_cnt) {
		if (cmd_resp[cmd_resp_ind].cmd_size == 0) {
			if (cmd_resp[cmd_resp_ind].rsp_size) {
				mock_uart_send(uart_dev, 
					      cmd_resp[cmd_resp_ind].rsp, 
					      cmd_resp[cmd_resp_ind].rsp_size);
			}
			cmd_resp_ind++;
			if (cmd_resp_ind == cmd_resp_cnt) {
				k_sem_give(&gnss_sem);
			}
		}
	}

	return 0;
}

/**
 * @brief GNSS device simulator thread, handles expected command&response 
 *        buffer and communication
 *
 * @param[in] dev Unused
 * 
 * @return 0 if everything was ok, error code otherwise
 */
static void gnss_dev_sim(void* dev)
{
	uint32_t size = 0;
	uint32_t total_size = 0;
	uint8_t data[100];

	while (true)
	{
		if (k_sem_take(&gnss_rx_sem, K_MSEC(10)) == 0) {
			mock_uart_receive(uart_dev, 
					  &data[total_size], 
					  &size, 
					  sizeof(data),
					  true);
			total_size += size;
#ifdef GNSS_VERBOSE
			printk("Length: %d\r\n", total_size);
#endif
			if (gnss_process_cmd_resp(data, total_size) == 0) {
				total_size = 0;
			}
		} else {
			gnss_process_empty_cmd_resp();
		}
	}
}

/**
 * @brief Testing GNSS setup functionality, and change from default baudrate 
 *        to configured baudrate 
 */
static void test_setup(void)
{
	/* Get baudrate (responding 38400) */
	uint8_t cmd_cfg_getval_baudrate[] = {0xB5, 0x62, 0x06, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x52, 0x40, 0x2C, 0x79};
	uint8_t resp_cfg_getval_baudrate[] = {0xB5, 0x62, 0x06, 0x8B, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x52, 0x40, 0x00, 0x96, 0x00, 0x00, 0xc6, 0x23, \
					      0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8B, 0x99, 0xC2};
	gnss_add_expected_cmd_rsp(cmd_cfg_getval_baudrate, 
				  sizeof(cmd_cfg_getval_baudrate), 
				  resp_cfg_getval_baudrate,
				  sizeof(resp_cfg_getval_baudrate));

	/* Change baudrate to 115200 */
	uint8_t cmd_cfg_setval_baudrate[] = {0xB5, 0x62, 0x06, 0x8A, 0x0C, 0x00, 0x00, 0x03, 0x00, 0x00, 0x01, 0x00, 0x52, 0x40, 0x00, 0xC2, 0x01, 0x00, 0xF5, 0xBB};
	uint8_t resp_cfg_setval_baudrate[] = {0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8A, 0x98, 0xC1};
	gnss_add_expected_cmd_rsp(cmd_cfg_setval_baudrate, 
				  sizeof(cmd_cfg_setval_baudrate), 
				  resp_cfg_setval_baudrate,
				  sizeof(resp_cfg_setval_baudrate));

	/* Disable NMEA output on UART */
	uint8_t cmd_cfg_setval_nmea_disable[] = {0xB5, 0x62, 0x06, 0x8A, 0x09, 0x00, 0x00, 0x03, 0x00, 0x00, 0x02, 0x00, 0x74, 0x10, 0x00, 0x22, 0xC7};
	uint8_t resp_cfg_setval_nmea_disable[] = {0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8A, 0x98, 0xC1};
	gnss_add_expected_cmd_rsp(cmd_cfg_setval_nmea_disable, 
				  sizeof(cmd_cfg_setval_nmea_disable), 
				  resp_cfg_setval_nmea_disable,
				  sizeof(resp_cfg_setval_nmea_disable));

	/* Enable NAV-PVT output on UART */
	uint8_t cmd_cfg_setval_nav_pvt[] = {0xB5, 0x62, 0x06, 0x8A, 0x09, 0x00, 0x00, 0x03, 0x00, 0x00, 0x07, 0x00, 0x91, 0x20, 0x01, 0x55, 0x58};
	uint8_t resp_cfg_setval_nav_pvt[] = {0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8A, 0x98, 0xC1};
	gnss_add_expected_cmd_rsp(cmd_cfg_setval_nav_pvt, 
				  sizeof(cmd_cfg_setval_nav_pvt), 
				  resp_cfg_setval_nav_pvt,
				  sizeof(resp_cfg_setval_nav_pvt));

	/* Enable NAV-DOP output on UART */
	uint8_t cmd_cfg_setval_nav_dop[] = {0xB5, 0x62, 0x06, 0x8A, 0x09, 0x00, 0x00, 0x03, 0x00, 0x00, 0x39, 0x00, 0x91, 0x20, 0x01, 0x87, 0x52};
	uint8_t resp_cfg_setval_nav_dop[] = {0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8A, 0x98, 0xC1};
	gnss_add_expected_cmd_rsp(cmd_cfg_setval_nav_dop, 
				  sizeof(cmd_cfg_setval_nav_dop), 
				  resp_cfg_setval_nav_dop,
				  sizeof(resp_cfg_setval_nav_dop));
	
	/* Enable NAV-STATUS output on UART */
	uint8_t cmd_cfg_setval_nav_status[] = {0xB5, 0x62, 0x06, 0x8A, 0x09, 0x00, 0x00, 0x03, 0x00, 0x00, 0x1B, 0x00, 0x91, 0x20, 0x01, 0x69, 0xBC};
	uint8_t resp_cfg_setval_nav_status[] = {0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8A, 0x98, 0xC1};
	gnss_add_expected_cmd_rsp(cmd_cfg_setval_nav_status, 
				  sizeof(cmd_cfg_setval_nav_status), 
				  resp_cfg_setval_nav_status,
				  sizeof(resp_cfg_setval_nav_status));

	/* Enable NAV-SAT output on UART, no handler */
	uint8_t cmd_cfg_setval_nav_sat[] = {0xB5, 0x62, 0x06, 0x8A, 0x09, 0x00, 0x00, 0x03, 0x00, 0x00, 0x16, 0x00, 0x91, 0x20, 0x01, 0x64, 0xa3};
	uint8_t resp_cfg_setval_nav_sat[] = {0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8A, 0x98, 0xC1};
	gnss_add_expected_cmd_rsp(cmd_cfg_setval_nav_sat, 
				  sizeof(cmd_cfg_setval_nav_sat), 
				  resp_cfg_setval_nav_sat,
				  sizeof(resp_cfg_setval_nav_sat));

	zassert_equal(gnss_setup(gnss_dev, true), 0, "GNSS Setup failed");
	zassert_equal(k_sem_take(&gnss_sem, K_MSEC(1000)), 0, 
		      "GNSS simulator did not indicate completion");
}

/**
 * @brief Testing GNSS setup functionality, and that GNSS baudrate 
 *        is not changed if already correct. 
 */
static void test_setup_2(void)
{	
	gnss_clear_expected();

	/* Get baudrate (responding 115200) */
	uint8_t cmd_cfg_getval_baudrate[] = {0xB5, 0x62, 0x06, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x52, 0x40, 0x2C, 0x79};
	uint8_t resp_cfg_getval_baudrate[] = {0xB5, 0x62, 0x06, 0x8B, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x52, 0x40, 0x00, 0xC2, 0x01, 0x00, 0xF3, 0xA9, \
					      0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8B, 0x99, 0xC2};
	gnss_add_expected_cmd_rsp(cmd_cfg_getval_baudrate, 
				  sizeof(cmd_cfg_getval_baudrate), 
				  resp_cfg_getval_baudrate,
				  sizeof(resp_cfg_getval_baudrate));

	/* Disable NMEA output on UART */
	uint8_t cmd_cfg_setval_nmea_disable[] = {0xB5, 0x62, 0x06, 0x8A, 0x09, 0x00, 0x00, 0x03, 0x00, 0x00, 0x02, 0x00, 0x74, 0x10, 0x00, 0x22, 0xC7};
	uint8_t resp_cfg_setval_nmea_disable[] = {0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8A, 0x98, 0xC1};
	gnss_add_expected_cmd_rsp(cmd_cfg_setval_nmea_disable, 
				  sizeof(cmd_cfg_setval_nmea_disable), 
				  resp_cfg_setval_nmea_disable,
				  sizeof(resp_cfg_setval_nmea_disable));

	/* Enable NAV-PVT output on UART */
	uint8_t cmd_cfg_setval_nav_pvt[] = {0xB5, 0x62, 0x06, 0x8A, 0x09, 0x00, 0x00, 0x03, 0x00, 0x00, 0x07, 0x00, 0x91, 0x20, 0x01, 0x55, 0x58};
	uint8_t resp_cfg_setval_nav_pvt[] = {0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8A, 0x98, 0xC1};
	gnss_add_expected_cmd_rsp(cmd_cfg_setval_nav_pvt, 
				  sizeof(cmd_cfg_setval_nav_pvt), 
				  resp_cfg_setval_nav_pvt,
				  sizeof(resp_cfg_setval_nav_pvt));

	/* Enable NAV-DOP output on UART */
	uint8_t cmd_cfg_setval_nav_dop[] = {0xB5, 0x62, 0x06, 0x8A, 0x09, 0x00, 0x00, 0x03, 0x00, 0x00, 0x39, 0x00, 0x91, 0x20, 0x01, 0x87, 0x52};
	uint8_t resp_cfg_setval_nav_dop[] = {0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8A, 0x98, 0xC1};
	gnss_add_expected_cmd_rsp(cmd_cfg_setval_nav_dop, 
				  sizeof(cmd_cfg_setval_nav_dop), 
				  resp_cfg_setval_nav_dop,
				  sizeof(resp_cfg_setval_nav_dop));
	
	/* Enable NAV-STATUS output on UART */
	uint8_t cmd_cfg_setval_nav_status[] = {0xB5, 0x62, 0x06, 0x8A, 0x09, 0x00, 0x00, 0x03, 0x00, 0x00, 0x1B, 0x00, 0x91, 0x20, 0x01, 0x69, 0xBC};
	uint8_t resp_cfg_setval_nav_status[] = {0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8A, 0x98, 0xC1};
	gnss_add_expected_cmd_rsp(cmd_cfg_setval_nav_status, 
				  sizeof(cmd_cfg_setval_nav_status), 
				  resp_cfg_setval_nav_status,
				  sizeof(resp_cfg_setval_nav_status));

	/* Enable NAV-SAT output on UART, no handler */
	uint8_t cmd_cfg_setval_nav_sat[] = {0xB5, 0x62, 0x06, 0x8A, 0x09, 0x00, 0x00, 0x03, 0x00, 0x00, 0x16, 0x00, 0x91, 0x20, 0x01, 0x64, 0xa3};
	uint8_t resp_cfg_setval_nav_sat[] = {0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8A, 0x98, 0xC1};
	gnss_add_expected_cmd_rsp(cmd_cfg_setval_nav_sat, 
				  sizeof(cmd_cfg_setval_nav_sat), 
				  resp_cfg_setval_nav_sat,
				  sizeof(resp_cfg_setval_nav_sat));


	zassert_equal(gnss_setup(gnss_dev, false), 0, "GNSS Setup failed");
	zassert_equal(k_sem_take(&gnss_sem, K_MSEC(1000)), 0, 
		      "GNSS simulator did not indicate completion");
}

/**
 * @brief Testing GNSS setup functionality, and that communication failure 
 *        lead to retry. (does not specifically check baudrates in use). 
 */
static void test_setup_3(void)
{	
	gnss_clear_expected();

	/* Get baudrate (no response) */
	uint8_t cmd_cfg_getval_baudrate[] = {0xB5, 0x62, 0x06, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x52, 0x40, 0x2C, 0x79};
	gnss_add_expected_cmd_rsp(cmd_cfg_getval_baudrate, 
				  sizeof(cmd_cfg_getval_baudrate), 
				  NULL,
				  0);
				  
	/* Get baudrate (responding 115200) */
	uint8_t cmd_cfg_getval_baudrate_2[] = {0xB5, 0x62, 0x06, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x52, 0x40, 0x2C, 0x79};
	uint8_t resp_cfg_getval_baudrate_2[] = {0xB5, 0x62, 0x06, 0x8B, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x52, 0x40, 0x00, 0xC2, 0x01, 0x00, 0xF3, 0xA9, \
					      0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8B, 0x99, 0xC2};
	gnss_add_expected_cmd_rsp(cmd_cfg_getval_baudrate_2, 
				  sizeof(cmd_cfg_getval_baudrate_2), 
				  resp_cfg_getval_baudrate_2,
				  sizeof(resp_cfg_getval_baudrate_2));

	/* Disable NMEA output on UART */
	uint8_t cmd_cfg_setval_nmea_disable[] = {0xB5, 0x62, 0x06, 0x8A, 0x09, 0x00, 0x00, 0x03, 0x00, 0x00, 0x02, 0x00, 0x74, 0x10, 0x00, 0x22, 0xC7};
	uint8_t resp_cfg_setval_nmea_disable[] = {0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8A, 0x98, 0xC1};
	gnss_add_expected_cmd_rsp(cmd_cfg_setval_nmea_disable, 
				  sizeof(cmd_cfg_setval_nmea_disable), 
				  resp_cfg_setval_nmea_disable,
				  sizeof(resp_cfg_setval_nmea_disable));

	/* Enable NAV-PVT output on UART */
	uint8_t cmd_cfg_setval_nav_pvt[] = {0xB5, 0x62, 0x06, 0x8A, 0x09, 0x00, 0x00, 0x03, 0x00, 0x00, 0x07, 0x00, 0x91, 0x20, 0x01, 0x55, 0x58};
	uint8_t resp_cfg_setval_nav_pvt[] = {0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8A, 0x98, 0xC1};
	gnss_add_expected_cmd_rsp(cmd_cfg_setval_nav_pvt, 
				  sizeof(cmd_cfg_setval_nav_pvt), 
				  resp_cfg_setval_nav_pvt,
				  sizeof(resp_cfg_setval_nav_pvt));

	/* Enable NAV-DOP output on UART */
	uint8_t cmd_cfg_setval_nav_dop[] = {0xB5, 0x62, 0x06, 0x8A, 0x09, 0x00, 0x00, 0x03, 0x00, 0x00, 0x39, 0x00, 0x91, 0x20, 0x01, 0x87, 0x52};
	uint8_t resp_cfg_setval_nav_dop[] = {0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8A, 0x98, 0xC1};
	gnss_add_expected_cmd_rsp(cmd_cfg_setval_nav_dop, 
				  sizeof(cmd_cfg_setval_nav_dop), 
				  resp_cfg_setval_nav_dop,
				  sizeof(resp_cfg_setval_nav_dop));
	
	/* Enable NAV-STATUS output on UART */
	uint8_t cmd_cfg_setval_nav_status[] = {0xB5, 0x62, 0x06, 0x8A, 0x09, 0x00, 0x00, 0x03, 0x00, 0x00, 0x1B, 0x00, 0x91, 0x20, 0x01, 0x69, 0xBC};
	uint8_t resp_cfg_setval_nav_status[] = {0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8A, 0x98, 0xC1};
	gnss_add_expected_cmd_rsp(cmd_cfg_setval_nav_status, 
				  sizeof(cmd_cfg_setval_nav_status), 
				  resp_cfg_setval_nav_status,
				  sizeof(resp_cfg_setval_nav_status));

	/* Enable NAV-SAT output on UART, no handler */
	uint8_t cmd_cfg_setval_nav_sat[] = {0xB5, 0x62, 0x06, 0x8A, 0x09, 0x00, 0x00, 0x03, 0x00, 0x00, 0x16, 0x00, 0x91, 0x20, 0x01, 0x64, 0xa3};
	uint8_t resp_cfg_setval_nav_sat[] = {0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8A, 0x98, 0xC1};
	gnss_add_expected_cmd_rsp(cmd_cfg_setval_nav_sat, 
				  sizeof(cmd_cfg_setval_nav_sat), 
				  resp_cfg_setval_nav_sat,
				  sizeof(resp_cfg_setval_nav_sat));

	zassert_equal(gnss_setup(gnss_dev, true), 0, "GNSS Setup failed");
	zassert_equal(k_sem_take(&gnss_sem, K_MSEC(1000)), 0, 
		      "GNSS simulator did not indicate completion");
}

/**
 * @brief Testing GNSS solution receiving, callbacks and fetch functions. 
 */
static void test_receive_gnss_solution(void)
{
	gnss_t gnss_fetch_data;

	k_sem_init(&gnss_data_sem, 0, 1);

	/* Register function pointers for GNSS data callbacks */
	zassert_equal(gnss_set_data_cb(gnss_dev, gnss_data_cb), 0, 
		      "Tried registering data callback, got error");

	/* Check that we don't get any data before data is available */
	zassert_equal(gnss_data_fetch(gnss_dev, &gnss_fetch_data), -ENODATA, 
		      "Wrong error code for no GNSS data");

	uint8_t nav_pvt_01[] = {0xB5, 0x62, 0x01, 0x07, 0x5C, 0x00, 0x58, 0xF9, 0x28, 0x08, 0xE6, 0x07, 0x02, 0x15, 0x0E, 0x01, 0x19, 0x37, 0x1F, 0x00, 0x00, 0x00, 0xCA, 0x3A, 0xFA, 0xFF, 0x03, 0x01, 0xFA, 0x0E, 0x5C, 0xF2, 0x29, 0x06, 0xFC, 0x29, 0xC6, 0x25, 0x38, 0x0E, 0x03, 0x00, 0xBA, 0x71, 0x02, 0x00, 0xEC, 0x0A, 0x00, 0x00, 0x6B, 0x15, 0x00, 0x00, 0xF1, 0xFF, 0xFF, 0xFF, 0xFC, 0xFF, 0xFF, 0xFF, 0x0D, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x97, 0x01, 0x00, 0x00, 0x72, 0xD0, 0xE0, 0x00, 0xAD, 0x00, 0x00, 0x00, 0xD2, 0x7A, 0x5B, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1B, 0x8A};
	uint8_t nav_status_01[] = {0xB5, 0x62, 0x01, 0x03, 0x10, 0x00, 0x58, 0xF9, 0x28, 0x08, 0x03, 0xDD, 0x00, 0x0C, 0xE3, 0x77, 0x00, 0x00, 0xF7, 0xC5, 0x1C, 0x00, 0xB3, 0xE7};
	uint8_t nav_dop_01[] = {0xB5, 0x62, 0x01, 0x04, 0x12, 0x00, 0x58, 0xF9, 0x28, 0x08, 0xC7, 0x00, 0xAD, 0x00, 0x62, 0x00, 0x98, 0x00, 0x54, 0x00, 0x48, 0x00, 0x2C, 0x00, 0xCE, 0x85};

	mock_uart_send(uart_dev, nav_pvt_01, sizeof(nav_pvt_01));
	mock_uart_send(uart_dev, nav_status_01, sizeof(nav_status_01));
	mock_uart_send(uart_dev, nav_dop_01, sizeof(nav_dop_01));

	zassert_equal(k_sem_take(&gnss_data_sem, K_MSEC(1000)), 0, 
		      "Did not get GNSS data callback");

	zassert_equal(gnss_cb_data.latest.lat, 633743868, "Wrong latitude");
	zassert_equal(gnss_cb_data.latest.lon, 103412316, "Wrong latitude");

	zassert_equal(gnss_data_fetch(gnss_dev, &gnss_fetch_data), 0, 
		      "Fetching GNSS data failed");

	zassert_mem_equal(&gnss_fetch_data, &gnss_cb_data, 
			  sizeof(gnss_t), 
			  "Mismatch between GNSS data in callback and fetch");
}

/**
 * @brief Testing GNSS ignoring of NMEA data. 
 */
static void test_ignore_nmea(void)
{
	/* Make sure no data was received unexpectedly */
	zassert_equal(k_sem_take(&gnss_data_sem, K_NO_WAIT), -EBUSY , 
		      "Unexpected data");

	uint8_t nav_pvt_03[] = {0xB5, 0x62, 0x01, 0x07, 0x5C, 0x00, 0x28, 0x01, 0x29, 0x08, 0xE6, 0x07, 0x02, 0x15, 0x0E, 0x01, 0x1B, 0x37, 0x1F, 0x00, 0x00, 0x00, 0x82, 0x3D, 0xFA, 0xFF, 0x03, 0x01, 0xFA, 0x0E, 0x62, 0xF2, 0x29, 0x06, 0xF6, 0x29, 0xC6, 0x25, 0x89, 0x0E, 0x03, 0x00, 0x0B, 0x72, 0x02, 0x00, 0xE8, 0x0A, 0x00, 0x00, 0x67, 0x15, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5B, 0x01, 0x00, 0x00, 0xA2, 0xD9, 0xE0, 0x00, 0xAD, 0x00, 0x00, 0x00, 0xD2, 0x7A, 0x5B, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x7B};
	uint8_t nav_status_03[] = {0xB5, 0x62, 0x01, 0x03, 0x10, 0x00, 0x28, 0x01, 0x29, 0x08, 0x03, 0xDD, 0x00, 0x0C, 0xE3, 0x77, 0x00, 0x00, 0xC7, 0xCD, 0x1C, 0x00, 0x64, 0xC5};
	uint8_t nav_dop_03[] = {0xB5, 0x62, 0x01, 0x04, 0x12, 0x00, 0x28, 0x01, 0x29, 0x08, 0xC7, 0x00, 0xAD, 0x00, 0x62, 0x00, 0x98, 0x00, 0x54, 0x00, 0x48, 0x00, 0x2C, 0x00, 0xA7, 0xBD};

	/* Send only NAV-PVT */
	mock_uart_send(uart_dev, nav_pvt_03, sizeof(nav_pvt_03));
	zassert_equal(k_sem_take(&gnss_data_sem, K_MSEC(100)), -EAGAIN , 
		      "Data received unexpectedly");

	/* Send some well-formed NMEA data */
	char* nmea_data = "$GNGGA,173002.00,6322.46840,N,01020.45757,E,1,05,7.83,167.0,M,40.1,M,,*45\r\n";
	mock_uart_send(uart_dev, (uint8_t*)nmea_data, strlen(nmea_data));

	/* Still no data */
	zassert_equal(k_sem_take(&gnss_data_sem, K_MSEC(100)), -EAGAIN , 
		      "Data received unexpectedly");

	/* Send the remaining messages */
	mock_uart_send(uart_dev, nav_status_03, sizeof(nav_status_03));
	mock_uart_send(uart_dev, nav_dop_03, sizeof(nav_dop_03));

	/* Expect data now */
	zassert_equal(k_sem_take(&gnss_data_sem, K_MSEC(100)), 0 , 
		      "No data received");

	/* New data */
	uint8_t nav_pvt_04[] = {0xB5, 0x62, 0x01, 0x07, 0x5C, 0x00, 0x10, 0x05, 0x29, 0x08, 0xE6, 0x07, 0x02, 0x15, 0x0E, 0x01, 0x1C, 0x37, 0x1F, 0x00, 0x00, 0x00, 0xDE, 0x3E, 0xFA, 0xFF, 0x03, 0x01, 0xFA, 0x0E, 0x64, 0xF2, 0x29, 0x06, 0xF4, 0x29, 0xC6, 0x25, 0xCD, 0x0E, 0x03, 0x00, 0x4F, 0x72, 0x02, 0x00, 0xE4, 0x0A, 0x00, 0x00, 0x58, 0x15, 0x00, 0x00, 0xE6, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x00, 0x00, 0xF0, 0xFF, 0xFF, 0xFF, 0x1A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x01, 0x00, 0x00, 0x7A, 0xDD, 0xE0, 0x00, 0xAD, 0x00, 0x00, 0x00, 0xD2, 0x7A, 0x5B, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB3, 0xE0};
	uint8_t nav_status_04[] = {0xB5, 0x62, 0x01, 0x03, 0x10, 0x00, 0x10, 0x05, 0x29, 0x08, 0x03, 0xDD, 0x00, 0x0C, 0xE3, 0x77, 0x00, 0x00, 0xAF, 0xD1, 0x1C, 0x00, 0x3C, 0x2D};
	uint8_t nav_dop_04[] = {0xB5, 0x62, 0x01, 0x04, 0x12, 0x00, 0x10, 0x05, 0x29, 0x08, 0xC7, 0x00, 0xAD, 0x00, 0x62, 0x00, 0x98, 0x00, 0x54, 0x00, 0x48, 0x00, 0x2C, 0x00, 0x93, 0x51};

	/* Send only NAV-PVT */
	mock_uart_send(uart_dev, nav_pvt_04, sizeof(nav_pvt_04));
	zassert_equal(k_sem_take(&gnss_data_sem, K_MSEC(100)), -EAGAIN , 
		      "Data received unexpectedly");

	/* Send too long NMEA data, checksum is wrong, but it's okay since we won't use it anyways */
	char* nmea_long_data = "$GNGGA,173002.00000000000000000000000000,6322.468400000000000000000000000000000,N,01020.45757,E,1,05,7.83,167.0,M,40.1,M,,*45\r\n";
	mock_uart_send(uart_dev, (uint8_t*)nmea_long_data, 
		       strlen(nmea_long_data));

	/* Still no data */
	zassert_equal(k_sem_take(&gnss_data_sem, K_MSEC(100)), -EAGAIN , 
		      "Data received unexpectedly");

	/* Send the remaining messages */
	mock_uart_send(uart_dev, nav_status_04, sizeof(nav_status_04));
	mock_uart_send(uart_dev, nav_dop_04, sizeof(nav_dop_04));

	/* Expect data now */
	zassert_equal(k_sem_take(&gnss_data_sem, K_MSEC(100)), 0 , 
		      "No data received");
}

/**
 * @brief Testing GNSS ignoring of garbage data. 
 */
static void test_ignore_garbage(void)
{
	/* Make sure no data was received unexpectedly */
	zassert_equal(k_sem_take(&gnss_data_sem, K_NO_WAIT), -EBUSY , 
		      "Unexpected data");

	uint8_t nav_pvt_05[] = {0xB5, 0x62, 0x01, 0x07, 0x5C, 0x00, 0xF8, 0x08, 0x29, 0x08, 0xE6, 0x07, 0x02, 0x15, 0x0E, 0x01, 0x1D, 0x37, 0x1F, 0x00, 0x00, 0x00, 0x3A, 0x40, 0xFA, 0xFF, 0x03, 0x01, 0xFA, 0x0E, 0x66, 0xF2, 0x29, 0x06, 0xF1, 0x29, 0xC6, 0x25, 0xF1, 0x0E, 0x03, 0x00, 0x73, 0x72, 0x02, 0x00, 0xDE, 0x0A, 0x00, 0x00, 0x49, 0x15, 0x00, 0x00, 0xF6, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x72, 0x01, 0x00, 0x00, 0xE4, 0xE1, 0xE0, 0x00, 0xAD, 0x00, 0x00, 0x00, 0xD2, 0x7A, 0x5B, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE4, 0xA3};
	uint8_t nav_status_05[] = {0xB5, 0x62, 0x01, 0x03, 0x10, 0x00, 0xF8, 0x08, 0x29, 0x08, 0x03, 0xDD, 0x00, 0x0C, 0xE3, 0x77, 0x00, 0x00, 0x97, 0xD5, 0x1C, 0x00, 0x13, 0x86};
	uint8_t nav_dop_05[] = {0xB5, 0x62, 0x01, 0x04, 0x12, 0x00, 0xF8, 0x08, 0x29, 0x08, 0xC7, 0x00, 0xAD, 0x00, 0x62, 0x00, 0x97, 0x00, 0x54, 0x00, 0x48, 0x00, 0x2C, 0x00, 0x7D, 0xCC};

	/* Send only NAV-PVT and NAV-STATUS*/
	mock_uart_send(uart_dev, nav_pvt_05, sizeof(nav_pvt_05));
	mock_uart_send(uart_dev, nav_status_05, sizeof(nav_status_05));
	zassert_equal(k_sem_take(&gnss_data_sem, K_MSEC(100)), -EAGAIN , 
		      "Data received unexpectedly");

	/* Send some garbage data */
	uint8_t garbage[] = {0x13, 0x37, 0xB5, 0x55, 0x00, '3'};
	mock_uart_send(uart_dev, garbage, sizeof(garbage));

	/* Still no data */
	zassert_equal(k_sem_take(&gnss_data_sem, K_MSEC(100)), -EAGAIN , 
		      "Data received unexpectedly");

	/* Send final part of data */
	mock_uart_send(uart_dev, nav_dop_05, sizeof(nav_dop_05));

	/* Expect data now */
	zassert_equal(k_sem_take(&gnss_data_sem, K_MSEC(100)), 0 , 
		      "No data received");
}

/**
 * @brief Testing GNSS setting and getting of used measure rate
 */
static void test_set_get_rate(void)
{
	gnss_clear_expected();

	/* Expect valset for rate set command */
	uint8_t cmd_cfg_setval_rate[] = {0xB5, 0x62, 0x06, 0x8A, 0x0A, 0x00, 0x00, 0x03, 0x00, 0x00, 0x01, 0x00, 0x21, 0x30, 0xFA, 0x00, 0xE9, 0xF7};
	uint8_t resp_cfg_setval_rate[] = {0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8A, 0x98, 0xC1};
	gnss_add_expected_cmd_rsp(cmd_cfg_setval_rate, 
				  sizeof(cmd_cfg_setval_rate), 
				  resp_cfg_setval_rate,
				  sizeof(resp_cfg_setval_rate));

	/* Expect valget for rate get command */
	/* Response is split into multiple smaller segments to make sure 
	 * parser will correctly handled segmented data */
	uint8_t cmd_cfg_getval_rate[] = {0xB5, 0x62, 0x06, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x21, 0x30, 0xEB, 0x07};
	uint8_t resp_cfg_getval_rate_01[] = {0xB5};
	uint8_t resp_cfg_getval_rate_02[] = {0x62, 0x06, 0x8B};
	uint8_t resp_cfg_getval_rate_03[] = {0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
	uint8_t resp_cfg_getval_rate_04[] = {0x00, 0x21, 0x30, 0x39, 0x05, 0x2B, 0x6C, \
					      0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8B, 0x99, 0xC2};
	gnss_add_expected_cmd_rsp(cmd_cfg_getval_rate, 
				  sizeof(cmd_cfg_getval_rate), 
				  resp_cfg_getval_rate_01,
				  sizeof(resp_cfg_getval_rate_01));
	gnss_add_expected_cmd_rsp(NULL, 
				  0, 
				  resp_cfg_getval_rate_02,
				  sizeof(resp_cfg_getval_rate_02));
	gnss_add_expected_cmd_rsp(NULL, 
				  0, 
				  resp_cfg_getval_rate_03,
				  sizeof(resp_cfg_getval_rate_03));
	gnss_add_expected_cmd_rsp(NULL, 
				  0, 
				  resp_cfg_getval_rate_04,
				  sizeof(resp_cfg_getval_rate_04));
	
	/* Set rate to 250ms */
	zassert_equal(gnss_set_rate(gnss_dev, 250), 0, "GNSS set rate failed");
	
	/* Get rate, responds 1337ms */
	uint16_t rate = 0;
	zassert_equal(gnss_get_rate(gnss_dev, &rate), 0, 
		      "GNSS get rate failed");
	zassert_equal(rate, 1337, "GNSS get rate gave wrong value");

	zassert_equal(k_sem_take(&gnss_sem, K_MSEC(1000)), 0, 
		      "GNSS simulator did not indicate completion");
}

/**
 * @brief Testing GNSS ignoring messages with invalid checksum. 
 */
static void test_ignore_wrong_checksum(void)
{
	gnss_clear_expected();

	/* Expect valset for rate set command 
	 * invalid checksum in response ack
	 * real checksum ends with 0xC1 
	 */
	uint8_t cmd_cfg_setval_rate[] = {0xB5, 0x62, 0x06, 0x8A, 0x0A, 0x00, 0x00, 0x03, 0x00, 0x00, 0x01, 0x00, 0x21, 0x30, 0xFA, 0x00, 0xE9, 0xF7};
	uint8_t resp_cfg_setval_rate[] = {0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8A, 0x98, 0xC3}; 
	gnss_add_expected_cmd_rsp(cmd_cfg_setval_rate, 
				  sizeof(cmd_cfg_setval_rate), 
				  resp_cfg_setval_rate,
				  sizeof(resp_cfg_setval_rate));
	
	/* Set rate to 250ms */
	zassert_equal(gnss_set_rate(gnss_dev, 250), -ETIME, 
		      "GNSS set rate did not time out when wrong \
		      checksum in ack");
	zassert_equal(k_sem_take(&gnss_sem, K_MSEC(1000)), 0, 
		      "GNSS simulator did not indicate completion");
}

/**
 * @brief Testing GNSS reset. 
 */
static void test_reset(void)
{
	gnss_clear_expected();

	/* Expect warm reset command */
	uint8_t cmd_cfg_rst_warm[] = {0xB5, 0x62, 0x06, 0x04, 0x04, 0x00, 0x01, 0x00, 0x01, 0x00, 0x10, 0x6a};
	gnss_add_expected_cmd_rsp(cmd_cfg_rst_warm, 
				  sizeof(cmd_cfg_rst_warm), 
				  NULL,
				  0);

	/* Send warm reset using API */
	zassert_equal(gnss_reset(gnss_dev, 
				 GNSS_RESET_MASK_WARM, 
				 GNSS_RESET_MODE_SW), 
		      0 , "Reset failed");
	zassert_equal(k_sem_take(&gnss_sem, K_MSEC(1000)), 0, 
		      "GNSS simulator did not indicate completion");
}

/**
 * @brief Testing GNSS doesn't receive ack, but gets data. 
 */
static void test_no_ack(void)
{
	gnss_clear_expected();

	/* Expect valget for rate get, no ack, but has data */
	uint8_t cmd_cfg_getval_rate[] = {0xB5, 0x62, 0x06, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x21, 0x30, 0xEB, 0x07};
	uint8_t resp_cfg_getval_rate_no_ack[] = {0xB5, 0x62, 0x06, 0x8B, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x21, 0x30, 0x39, 0x05, 0x2B, 0x6C};
	gnss_add_expected_cmd_rsp(cmd_cfg_getval_rate, 
				  sizeof(cmd_cfg_getval_rate), 
				  resp_cfg_getval_rate_no_ack,
				  sizeof(resp_cfg_getval_rate_no_ack));

	/* Get rate, should timeout, and data unchanged */
	uint16_t rate = 0x55AA;
	zassert_equal(gnss_get_rate(gnss_dev, &rate), -ETIME, 
		      "GNSS get rate did not timeout");
	zassert_equal(rate, 0x55AA, 
		      "GNSS get rate modified rate even though it timed out");
	zassert_equal(k_sem_take(&gnss_sem, K_MSEC(1000)), 0, 
		      "GNSS simulator did not indicate completion");
}

/**
 * @brief Testing GNSS doesn't receive data, but gets ack. 
 */
static void test_no_resp(void)
{
	gnss_clear_expected();

	/* Expect valget for rate get, no data, but has ack */
	uint8_t cmd_cfg_getval_rate[] = {0xB5, 0x62, 0x06, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x21, 0x30, 0xEB, 0x07};
	uint8_t resp_cfg_getval_rate_no_data[] = {0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8B, 0x99, 0xC2};
	gnss_add_expected_cmd_rsp(cmd_cfg_getval_rate, 
				  sizeof(cmd_cfg_getval_rate), 
				  resp_cfg_getval_rate_no_data,
				  sizeof(resp_cfg_getval_rate_no_data));

	/* Get rate, should timeout, and data unchanged */
	uint16_t rate = 0x55AA;
	zassert_equal(gnss_get_rate(gnss_dev, &rate), -ETIME, 
		      "GNSS get rate did not timeout");
	zassert_equal(rate, 0x55AA, 
		      "GNSS get rate modified rate even though it timed out");
	zassert_equal(k_sem_take(&gnss_sem, K_MSEC(1000)), 0, 
		      "GNSS simulator did not indicate completion");
}

/**
 * @brief Testing GNSS upload of assistance data
 */
static void test_upload_assistance_data(void)
{
	gnss_clear_expected();

	/* Expect MGA-ANO for assistance data upload */
	uint8_t assistance_data[] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b};
	uint8_t cmd_mga_ano[] = {0xb5, 0x62, 0x13, 0x20, 0x4c, 0x0, 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0xa1, 0xb6};
	uint8_t rsp_mga_ano[] = {0xb5, 0x62, 0x5, 0x1, 0x2, 0x0, 0x13, 0x20, 0x3b, 0x71};
	gnss_add_expected_cmd_rsp(cmd_mga_ano, 
				  sizeof(cmd_mga_ano), 
				  rsp_mga_ano,
				  sizeof(rsp_mga_ano));

	/* Upload assistance data */
	zassert_equal(gnss_upload_assist_data(gnss_dev, 
					      assistance_data, 
					      sizeof(assistance_data)), 
		      0, "GNSS assistance upload failed");
	zassert_equal(k_sem_take(&gnss_sem, K_MSEC(1000)), 0, 
		      "GNSS simulator did not indicate completion");
}

void test_main(void)
{
	k_sem_init(&gnss_rx_sem, 0, 1);
	k_sem_init(&gnss_sem, 0, 1);
	
	mock_uart_register_sem(uart_dev, &gnss_rx_sem);
	
	/* Start dev thread */
	k_thread_create(&gnss_dev_thread, gnss_dev_stack,
			K_KERNEL_STACK_SIZEOF(gnss_dev_stack),
			(k_thread_entry_t) gnss_dev_sim,
			(void*)gnss_dev, NULL, NULL, 
			K_PRIO_COOP(2), 0, K_NO_WAIT);

	ztest_test_suite(common, ztest_unit_test(test_setup), 
				 ztest_unit_test(test_setup_2),
				 ztest_unit_test(test_setup_3),
				 ztest_unit_test(test_receive_gnss_solution),
				 ztest_unit_test(test_ignore_nmea),
				 ztest_unit_test(test_ignore_garbage),
				 ztest_unit_test(test_set_get_rate),
				 ztest_unit_test(test_ignore_wrong_checksum),
				 ztest_unit_test(test_reset),
				 ztest_unit_test(test_no_ack),
				 ztest_unit_test(test_no_resp),
				 ztest_unit_test(test_upload_assistance_data));
	ztest_run_test_suite(common);
}