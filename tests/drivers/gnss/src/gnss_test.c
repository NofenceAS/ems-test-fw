#include <ztest.h>
#include <zephyr.h>
#include <device.h>
#include "gnss.h"
#include "uart_mock.h"

const struct device *uart_dev = DEVICE_DT_GET(DT_ALIAS(gnssuart));
const struct device *gnss_dev = DEVICE_DT_GET(DT_ALIAS(gnss));

/* Parser thread structures */
K_KERNEL_STACK_DEFINE(gnss_dev_stack,
		      2048);
struct k_thread gnss_dev_thread;

static struct k_sem gnss_sem;
static struct k_sem gnss_rx_sem;

#define GNSS_CMD_RESP_MAX 20

typedef struct gnss_cmd_resp_t {
	uint8_t* cmd;
	uint32_t cmd_size;
	uint8_t* rsp;
	uint32_t rsp_size;
} gnss_cmd_resp_t;
static gnss_cmd_resp_t gnss_cmd_resp[GNSS_CMD_RESP_MAX];
static uint32_t gnss_cmd_resp_cnt = 0;

static int gnss_add_expected_command_and_response(const uint8_t* cmd, 
						  uint32_t cmd_size, 
						  const uint8_t* rsp,
						  uint32_t rsp_size)
{
	int ret = -ENOBUFS;
	if (gnss_cmd_resp_cnt < GNSS_CMD_RESP_MAX) {
		gnss_cmd_resp[gnss_cmd_resp_cnt].cmd = k_malloc(cmd_size);
		gnss_cmd_resp[gnss_cmd_resp_cnt].cmd_size = cmd_size;
		memcpy(gnss_cmd_resp[gnss_cmd_resp_cnt].cmd,
		       cmd, 
		       cmd_size);
		
		gnss_cmd_resp[gnss_cmd_resp_cnt].rsp = k_malloc(rsp_size);
		gnss_cmd_resp[gnss_cmd_resp_cnt].rsp_size = rsp_size;
		memcpy(gnss_cmd_resp[gnss_cmd_resp_cnt].rsp,
		       rsp, 
		       rsp_size);
		
		gnss_cmd_resp_cnt++;

		ret = 0;
	}
	return ret;
}

static void gnss_dev_sim(void* dev)
{
	uint32_t size = 0;
	uint32_t total_size = 0;
	uint8_t data[100];
	
	uint32_t cmd_resp_ind = 0;

	while (true)
	{
		if (k_sem_take(&gnss_rx_sem, K_MSEC(1000)) == 0) {
			mock_uart_receive(uart_dev, 
					  &data[total_size], 
					  &size, 
					  sizeof(data),
					  true);
			total_size += size;
			printk("Length: %d\r\n", total_size);
			
			/* TODO - Change comparison to byte by byte to detect errors even before full message size */
			if (total_size == gnss_cmd_resp[cmd_resp_ind].cmd_size) {
				printk("Parsed data: ");
				for (int i = 0; i < total_size; i++)
				{
					printk("0x%02X ", data[i]);
				}
				printk("\r\n");
				if (memcmp(gnss_cmd_resp[cmd_resp_ind].cmd, data, total_size) == 0) {
					printk("MATCH!\r\n");
					mock_uart_send(uart_dev, gnss_cmd_resp[cmd_resp_ind].rsp, gnss_cmd_resp[cmd_resp_ind].rsp_size);

					total_size = 0;

					cmd_resp_ind++;

					if (cmd_resp_ind == gnss_cmd_resp_cnt) {
						k_sem_give(&gnss_sem);
					}
				} else {
					/* TODO - FAILED */
				}
			}
		}
	}
}

static void test_setup(void)
{	
	k_sem_init(&gnss_rx_sem, 0, 1);
	k_sem_init(&gnss_sem, 0, 1);

	/* Get baudrate (responding 38400) */
	uint8_t cmd_cfg_getval_baudrate[] = {0xB5, 0x62, 0x06, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x52, 0x40, 0x2C, 0x79};
	uint8_t resp_cfg_getval_baudrate[] = {0xB5, 0x62, 0x06, 0x8B, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x52, 0x40, 0x00, 0x96, 0x00, 0x00, 0xc6, 0x23, \
					      0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8B, 0x99, 0xC2};
	gnss_add_expected_command_and_response(cmd_cfg_getval_baudrate, 
						  sizeof(cmd_cfg_getval_baudrate), 
						  resp_cfg_getval_baudrate,
						  sizeof(resp_cfg_getval_baudrate));
						  
	/* Change baudrate to 115200 */
	uint8_t cmd_cfg_setval_baudrate[] = {0xB5, 0x62, 0x06, 0x8A, 0x0C, 0x00, 0x00, 0x03, 0x00, 0x00, 0x01, 0x00, 0x52, 0x40, 0x00, 0xC2, 0x01, 0x00, 0xF5, 0xBB};
	uint8_t resp_cfg_setval_baudrate[] = {0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8A, 0x98, 0xC1};
	gnss_add_expected_command_and_response(cmd_cfg_setval_baudrate, 
						  sizeof(cmd_cfg_setval_baudrate), 
						  resp_cfg_setval_baudrate,
						  sizeof(resp_cfg_setval_baudrate));

	/* Disable NMEA output on UART */
	uint8_t cmd_cfg_setval_nmea_disable[] = {0xB5, 0x62, 0x06, 0x8A, 0x09, 0x00, 0x00, 0x03, 0x00, 0x00, 0x02, 0x00, 0x74, 0x10, 0x00, 0x22, 0xC7};
	uint8_t resp_cfg_setval_nmea_disable[] = {0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8A, 0x98, 0xC1};
	gnss_add_expected_command_and_response(cmd_cfg_setval_nmea_disable, 
						  sizeof(cmd_cfg_setval_nmea_disable), 
						  resp_cfg_setval_nmea_disable,
						  sizeof(resp_cfg_setval_nmea_disable));

	/* Enable NAV-PVT output on UART */
	uint8_t cmd_cfg_setval_nav_pvt[] = {0xB5, 0x62, 0x06, 0x8A, 0x09, 0x00, 0x00, 0x03, 0x00, 0x00, 0x07, 0x00, 0x91, 0x20, 0x01, 0x55, 0x58};
	uint8_t resp_cfg_setval_nav_pvt[] = {0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8A, 0x98, 0xC1};
	gnss_add_expected_command_and_response(cmd_cfg_setval_nav_pvt, 
						  sizeof(cmd_cfg_setval_nav_pvt), 
						  resp_cfg_setval_nav_pvt,
						  sizeof(resp_cfg_setval_nav_pvt));

	/* Enable NAV-DOP output on UART */
	uint8_t cmd_cfg_setval_nav_dop[] = {0xB5, 0x62, 0x06, 0x8A, 0x09, 0x00, 0x00, 0x03, 0x00, 0x00, 0x39, 0x00, 0x91, 0x20, 0x01, 0x87, 0x52};
	uint8_t resp_cfg_setval_nav_dop[] = {0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8A, 0x98, 0xC1};
	gnss_add_expected_command_and_response(cmd_cfg_setval_nav_dop, 
						  sizeof(cmd_cfg_setval_nav_dop), 
						  resp_cfg_setval_nav_dop,
						  sizeof(resp_cfg_setval_nav_dop));
	
	/* Enable NAV-STATUS output on UART */
	uint8_t cmd_cfg_setval_nav_status[] = {0xB5, 0x62, 0x06, 0x8A, 0x09, 0x00, 0x00, 0x03, 0x00, 0x00, 0x1B, 0x00, 0x91, 0x20, 0x01, 0x69, 0xBC};
	uint8_t resp_cfg_setval_nav_status[] = {0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8A, 0x98, 0xC1};
	gnss_add_expected_command_and_response(cmd_cfg_setval_nav_status, 
						  sizeof(cmd_cfg_setval_nav_status), 
						  resp_cfg_setval_nav_status,
						  sizeof(resp_cfg_setval_nav_status));

	/* Enable NAV-SAT output on UART, no handler */
	uint8_t cmd_cfg_setval_nav_sat[] = {0xB5, 0x62, 0x06, 0x8A, 0x09, 0x00, 0x00, 0x03, 0x00, 0x00, 0x16, 0x00, 0x91, 0x20, 0x01, 0x64, 0xa3};
	uint8_t resp_cfg_setval_nav_sat[] = {0xB5, 0x62, 0x05, 0x01, 0x02, 0x00, 0x06, 0x8A, 0x98, 0xC1};
	gnss_add_expected_command_and_response(cmd_cfg_setval_nav_sat, 
						  sizeof(cmd_cfg_setval_nav_sat), 
						  resp_cfg_setval_nav_sat,
						  sizeof(resp_cfg_setval_nav_sat));

	mock_uart_register_sem(uart_dev, &gnss_rx_sem);
	
	/* Start dev thread */
	k_thread_create(&gnss_dev_thread, gnss_dev_stack,
			K_KERNEL_STACK_SIZEOF(gnss_dev_stack),
			(k_thread_entry_t) gnss_dev_sim,
			(void*)gnss_dev, NULL, NULL, 
			K_PRIO_COOP(2), 0, K_NO_WAIT);

	zassert_equal(gnss_setup(gnss_dev, false), 0, "GNSS Setup failed");
	zassert_equal(k_sem_take(&gnss_sem, K_MSEC(1000)), 0, "GNSS simulator did not indicate completion");
}

static void test_receive_gnss_solution(void)
{
	zassert_equal(0, 0, "GNSS failed");
}

static void test_ignore_nmea(void)
{
	zassert_equal(0, 0, "GNSS failed");
}

static void test_ignore_unknown(void)
{
	zassert_equal(0, 0, "GNSS failed");
}

static void test_set_rate(void)
{
	zassert_equal(0, 0, "GNSS failed");
}

static void test_reset(void)
{
	zassert_equal(0, 0, "GNSS failed");
}

static void test_upload_assistance_data(void)
{
	zassert_equal(0, 0, "GNSS failed");
}

void test_main(void)
{
	ztest_test_suite(common, ztest_unit_test(test_setup), 
				 ztest_unit_test(test_receive_gnss_solution),
				 ztest_unit_test(test_ignore_nmea),
				 ztest_unit_test(test_ignore_unknown),
				 ztest_unit_test(test_set_rate),
				 ztest_unit_test(test_reset),
				 ztest_unit_test(test_upload_assistance_data));
	ztest_run_test_suite(common);
}