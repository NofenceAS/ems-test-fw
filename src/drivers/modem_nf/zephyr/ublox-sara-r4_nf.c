/*
 * Copyright (c) 2019-2020 Foundries.io
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT u_blox_sara_r4

#include <logging/log.h>
LOG_MODULE_REGISTER(modem_ublox_sara_r4_nf, CONFIG_MODEM_LOG_LEVEL);

#include <kernel.h>
#include <ctype.h>
#include <errno.h>
#include <zephyr.h>
#include <drivers/gpio.h>
#include <device.h>
#include <init.h>
#include <fcntl.h>
#include <pm/device.h>
#include <pm/device_runtime.h>
#include <net/net_if.h>
#include <net/net_offload.h>
#include <net/socket_offload.h>

#if defined(CONFIG_MODEM_UBLOX_SARA_AUTODETECT_APN)
#include <stdio.h>
#endif

#include "modem_context.h"
#include "modem_socket.h"
#include "modem_cmd_handler.h"
#include "modem_iface_uart.h"
#include <modem_nf.h>

#if !defined(CONFIG_MODEM_UBLOX_SARA_R4_MANUAL_MCCMNO)
#define CONFIG_MODEM_UBLOX_SARA_R4_MANUAL_MCCMNO ""
#endif

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
#include "tls_internal.h"
#include <net/tls_credentials.h>
#endif

/* pin settings */
enum mdm_control_pins {
	MDM_POWER = 0,
#if DT_INST_NODE_HAS_PROP(0, mdm_reset_gpios)
	MDM_RESET,
#endif
#if DT_INST_NODE_HAS_PROP(0, mdm_vint_gpios)
	MDM_VINT,
#endif
};
extern struct k_sem listen_sem;

#define STATUS_UPGRADE_RUNNING 0x01
#define STATUS_UPGRADE_NOT_RUNNING 0xFF
static struct modem_pin modem_pins[] = {
	/* MDM_POWER */
	MODEM_PIN(DT_INST_GPIO_LABEL(0, mdm_power_gpios), DT_INST_GPIO_PIN(0, mdm_power_gpios),
		  DT_INST_GPIO_FLAGS(0, mdm_power_gpios) | GPIO_OUTPUT),

#if DT_INST_NODE_HAS_PROP(0, mdm_reset_gpios)
	/* MDM_RESET */
	MODEM_PIN(DT_INST_GPIO_LABEL(0, mdm_reset_gpios), DT_INST_GPIO_PIN(0, mdm_reset_gpios),
		  DT_INST_GPIO_FLAGS(0, mdm_reset_gpios) | GPIO_OUTPUT),
#endif

#if DT_INST_NODE_HAS_PROP(0, mdm_vint_gpios)
	/* MDM_VINT */
	MODEM_PIN(DT_INST_GPIO_LABEL(0, mdm_vint_gpios), DT_INST_GPIO_PIN(0, mdm_vint_gpios),
		  DT_INST_GPIO_FLAGS(0, mdm_vint_gpios) | GPIO_INPUT),
#endif
};

#define MDM_UART_NODE DT_INST_BUS(0)
#define MDM_UART_DEV DEVICE_DT_GET(MDM_UART_NODE)

#define MDM_POWER_ENABLE 1
#define MDM_POWER_DISABLE 0
#define MDM_RESET_NOT_ASSERTED 1
#define MDM_RESET_ASSERTED 0

#define MDM_AT_CMD_TIMEOUT K_MSEC(200) /*UPSV=0 sometimes times out with 30MS*/
#define MDM_CMD_TIMEOUT K_SECONDS(10)
#define MDM_CMD_USOCL_TIMEOUT K_SECONDS(30)
#define MDM_DNS_TIMEOUT K_SECONDS(70)
#define MDM_CMD_CONN_TIMEOUT K_SECONDS(120)
#define MDM_REGISTRATION_TIMEOUT K_SECONDS(80)
#define MDM_PROMPT_CMD_DELAY K_MSEC(50)
#define MDM_PWR_OFF_CMD_TIMEOUT K_SECONDS(40)

#define MDM_MAX_DATA_LENGTH 512
#define MDM_RECV_MAX_BUF 64
#define MDM_RECV_BUF_SIZE 512

#define MDM_MAX_SOCKETS 6
#define MDM_BASE_SOCKET_NUM 0

#define MDM_MIN_ALLOWED_RSSI 5

#define MDM_NETWORK_RETRY_COUNT 3
#define MDM_WAIT_FOR_RSSI_COUNT 10
#define MDM_WAIT_FOR_RSSI_DELAY K_SECONDS(2)

#define MDM_MANUFACTURER_LENGTH 10
#define MDM_MODEL_LENGTH 16
#define MDM_REVISION_LENGTH 64
#define MDM_INFORMATION_LENGTH 32
#define MDM_IMEI_LENGTH 16
#define MDM_IMSI_LENGTH 16
#define MDM_APN_LENGTH 32
#define MDM_MAX_CERT_LENGTH 8192
#if defined(CONFIG_MODEM_UBLOX_SARA_AUTODETECT_VARIANT)
#define MDM_VARIANT_UBLOX_R4 4
#define MDM_VARIANT_UBLOX_U2 2
#endif

NET_BUF_POOL_DEFINE(mdm_recv_pool, MDM_RECV_MAX_BUF, MDM_RECV_BUF_SIZE, 0, NULL);

/* RX thread structures */
K_KERNEL_STACK_DEFINE(modem_rx_stack, CONFIG_MODEM_UBLOX_SARA_R4_RX_STACK_SIZE);
struct k_thread modem_rx_thread;

#if defined(CONFIG_MODEM_UBLOX_SARA_RSSI_WORK)
/* RX thread work queue */
K_KERNEL_STACK_DEFINE(modem_workq_stack, CONFIG_MODEM_UBLOX_SARA_R4_RX_WORKQ_STACK_SIZE);
static struct k_work_q modem_workq;
#endif

/* socket read callback data */
struct socket_read_data {
	char *recv_buf;
	size_t recv_buf_len;
	struct sockaddr *recv_addr;
	uint16_t recv_read_len;
};

/* driver data */
struct modem_data {
	struct net_if *net_iface;
	uint8_t mac_addr[6];

	/* modem interface */
	struct modem_iface_uart_data iface_data;
	uint8_t iface_rb_buf[MDM_MAX_DATA_LENGTH];

	/* modem cmds */
	struct modem_cmd_handler_data cmd_handler_data;
	uint8_t cmd_match_buf[MDM_RECV_BUF_SIZE];

	/* socket data */
	struct modem_socket_config socket_config;
	struct modem_socket sockets[MDM_MAX_SOCKETS];

#if defined(CONFIG_MODEM_UBLOX_SARA_RSSI_WORK)
	/* RSSI work */
	struct k_work_delayable rssi_query_work;
#endif

	/* modem data */
	char mdm_manufacturer[MDM_MANUFACTURER_LENGTH];
	char mdm_model[MDM_MODEL_LENGTH];
	char mdm_information[MDM_INFORMATION_LENGTH];
	char mdm_revision[MDM_REVISION_LENGTH];
	char mdm_imei[MDM_IMEI_LENGTH];
	char mdm_imsi[MDM_IMSI_LENGTH];
	char mdm_ccid[2 * MDM_IMSI_LENGTH];
	char mdm_pdp_addr[2 * MDM_IMSI_LENGTH];
	int upsv_state;
	int last_sock;
	int session_rat;
	int mnc;
	bool pdp_active;
	int rssi;
	int min_rssi;
	int max_rssi;

#if defined(CONFIG_MODEM_UBLOX_SARA_AUTODETECT_VARIANT)
	/* modem variant */
	int mdm_variant;
#endif
#if defined(CONFIG_MODEM_UBLOX_SARA_AUTODETECT_APN)
	/* APN */
	char mdm_apn[MDM_APN_LENGTH];
#endif

	/* modem state */
	int ev_creg;

	/* bytes written to socket in last transaction */
	int sock_written;

	/* response semaphore */
	struct k_sem sem_response;

	/* prompt semaphore */
	struct k_sem sem_prompt;

	/* FTP result semaphore */
	struct k_sem sem_ftp;

	/* Last FTP opcode fetched via the +UUFTPC URC */
	int ftp_op_code;

	/* Last FTP result fetched via the +UUFTPC URC */
	int ftp_result;

	/* FW install result semaphore */
	struct k_sem sem_fw_install;
};

static struct modem_data mdata;
static struct modem_context mctx;
static bool stop_rssi_work = false;
static int wake_up(void);
static int (*modem_read_status_cb)(uint8_t *status) = NULL;
static int (*modem_write_status_cb)(uint8_t status) = NULL;
static int modem_read_status(uint8_t *status);
static int modem_write_status(uint8_t status);
//#if defined(CONFIG_DNS_RESOLVER)
static struct zsock_addrinfo result;
static struct sockaddr result_addr;
static char result_canonname[DNS_MAX_NAME_SIZE + 1];
static bool ccid_ready = false;
//#endif
//
/* helper macro to keep readability */
#define ATOI(s_, value_, desc_) modem_atoi(s_, value_, desc_, __func__)

/**
 * @brief  Convert string to long integer, but handle errors
 *
 * @param  s: string with representation of integer number
 * @param  err_value: on error return this value instead
 * @param  desc: name the string being converted
 * @param  func: function where this is called (typically __func__)
 *
 * @retval return integer conversion on success, or err_value on error
 */
static int modem_atoi(const char *s, const int err_value, const char *desc, const char *func)
{
	int ret;
	char *endptr;

	ret = (int)strtol(s, &endptr, 10);
	if (!endptr || *endptr != '\0') {
		LOG_ERR("bad %s '%s' in %s", log_strdup(s), log_strdup(desc), log_strdup(func));
		return err_value;
	}

	return ret;
}

#if defined(CONFIG_MODEM_UBLOX_SARA_AUTODETECT_APN)

/* the list of SIM profiles. Global scope, so the app can change it */
const char *modem_sim_profiles = CONFIG_MODEM_UBLOX_SARA_AUTODETECT_APN_PROFILES;

int find_apn(char *apn, int apnlen, const char *profiles, const char *imsi)
{
	int rc = -1;

	/* try to find a match */
	char *s = strstr(profiles, imsi);

	if (s) {
		char *eos;

		/* find the assignment operator preceding the match */
		while (s >= profiles && !strchr("=", *s)) {
			s--;
		}
		/* find the apn preceding the assignment operator */
		while (s >= profiles && strchr(" =", *s)) {
			s--;
		}

		/* mark end of apn string */
		eos = s + 1;

		/* find first character of the apn */
		while (s >= profiles && !strchr(" ,", *s)) {
			s--;
		}
		s++;

		/* copy the key */
		if (s >= profiles) {
			int len = eos - s;

			if (len < apnlen) {
				memcpy(apn, s, len);
				apn[len] = '\0';
				rc = 0;
			} else {
				LOG_ERR("buffer overflow");
			}
		}
	}

	return rc;
}

/* try to detect APN automatically, based on IMSI */
int modem_detect_apn(const char *imsi)
{
	int rc = -1;

	if (imsi != NULL && strlen(imsi) >= 5) {
		/* extract MMC and MNC from IMSI */
		char mmcmnc[6];
		*mmcmnc = 0;
		strncat(mmcmnc, imsi, sizeof(mmcmnc) - 1);

		/* try to find a matching IMSI, and assign the APN */
		rc = find_apn(mdata.mdm_apn, sizeof(mdata.mdm_apn), modem_sim_profiles, mmcmnc);
		if (rc < 0) {
			rc = find_apn(mdata.mdm_apn, sizeof(mdata.mdm_apn), modem_sim_profiles,
				      "*");
		}
	}

	if (rc == 0) {
		LOG_INF("Assign APN: \"%s\"", log_strdup(mdata.mdm_apn));
	}

	return rc;
}
#endif

/* Forward declaration */
MODEM_CMD_DEFINE(on_cmd_sockwrite);

/* send binary data via the +USO[ST/WR] commands */
static ssize_t send_socket_data(void *obj, const struct msghdr *msg, k_timeout_t timeout)
{
	int ret;
	char send_buf[sizeof("AT+USO**=#,!###.###.###.###!,#####,####\r\n")];
	uint16_t dst_port = 0U;
	struct modem_socket *sock = (struct modem_socket *)obj;
	const struct modem_cmd handler_cmds[] = {
		MODEM_CMD("+USOST: ", on_cmd_sockwrite, 2U, ","),
		MODEM_CMD("+USOWR: ", on_cmd_sockwrite, 2U, ","),
	};
	struct sockaddr *dst_addr = msg->msg_name;
	size_t buf_len = 0;

	if (!sock) {
		return -EINVAL;
	}

	for (int i = 0; i < msg->msg_iovlen; i++) {
		if (!msg->msg_iov[i].iov_base || msg->msg_iov[i].iov_len == 0) {
			errno = EINVAL;
			return -1;
		}
		buf_len += msg->msg_iov[i].iov_len;
	}

	if (!sock->is_connected && sock->ip_proto != IPPROTO_UDP) {
		errno = ENOTCONN;
		return -1;
	}

	if (!dst_addr && sock->ip_proto == IPPROTO_UDP) {
		dst_addr = &sock->dst;
	}

	/*
	 * Binary and ASCII mode allows sending MDM_MAX_DATA_LENGTH bytes to
	 * the socket in one command
	 */
	if (buf_len > MDM_MAX_DATA_LENGTH) {
		buf_len = MDM_MAX_DATA_LENGTH;
	}

	/* The number of bytes written will be reported by the modem */
	mdata.sock_written = 0;

	if (sock->ip_proto == IPPROTO_UDP) {
		ret = modem_context_get_addr_port(dst_addr, &dst_port);
		snprintk(send_buf, sizeof(send_buf), "AT+USOST=%d,\"%s\",%u,%zu", sock->id,
			 modem_context_sprint_ip_addr(dst_addr), dst_port, buf_len);
	} else {
		snprintk(send_buf, sizeof(send_buf), "AT+USOWR=%d,%zu", sock->id, buf_len);
	}

	k_sem_take(&mdata.cmd_handler_data.sem_tx_lock, K_FOREVER);

	/* Reset prompt '@' semaphore */
	k_sem_reset(&mdata.sem_prompt);
	ret = modem_cmd_send_nolock(&mctx.iface, &mctx.cmd_handler, NULL, 0U, send_buf, NULL,
				    K_NO_WAIT);
	if (ret < 0) {
		goto exit;
	}

	/* set command handlers */
	ret = modem_cmd_handler_update_cmds(&mdata.cmd_handler_data, handler_cmds,
					    ARRAY_SIZE(handler_cmds), true);
	if (ret < 0) {
		goto exit;
	}

	/* Wait for prompt '@' */
	ret = k_sem_take(&mdata.sem_prompt, K_SECONDS(1));
	if (ret != 0) {
		ret = -ETIMEDOUT;
		LOG_ERR("No @ prompt received");
		goto exit;
	}

	/*
	 * The AT commands manual requires a 50 ms wait
	 * after '@' prompt if using AT+USOWR, but not
	 * if using AT+USOST. This if condition is matched with
	 * the command selection above.
	 */
	if (sock->ip_proto != IPPROTO_UDP) {
		k_sleep(MDM_PROMPT_CMD_DELAY);
	}

	/* Reset response semaphore before sending data
	 * So that we are sure that we won't use a previously pending one
	 * And we won't miss the one that is going to be freed
	 */
	k_sem_reset(&mdata.sem_response);

	/* Send data directly on modem iface */
	for (int i = 0; i < msg->msg_iovlen; i++) {
		int len = MIN(buf_len, msg->msg_iov[i].iov_len);

		if (len == 0) {
			break;
		}
		mctx.iface.write(&mctx.iface, msg->msg_iov[i].iov_base, len);
		buf_len -= len;
	}

	if (K_TIMEOUT_EQ(timeout, K_NO_WAIT)) {
		ret = 0;
		goto exit;
	}
	ret = k_sem_take(&mdata.sem_response, timeout);

	if (ret == 0) {
		ret = modem_cmd_handler_get_error(&mdata.cmd_handler_data);
	} else if (ret == -EAGAIN) {
		ret = -ETIMEDOUT;
	}

exit:
	/* unset handler commands and ignore any errors */
	(void)modem_cmd_handler_update_cmds(&mdata.cmd_handler_data, NULL, 0U, false);
	k_sem_give(&mdata.cmd_handler_data.sem_tx_lock);

	if (ret < 0) {
		return ret;
	}

	return mdata.sock_written;
}

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
/* send binary data via the +USO[ST/WR] commands */
static ssize_t send_cert(struct modem_socket *sock, struct modem_cmd *handler_cmds,
			 size_t handler_cmds_len, const char *cert_data, size_t cert_len,
			 int cert_type)
{
	int ret;
	char *filename = "ca";
	char send_buf[sizeof("AT+USECMNG=#,#,!####!,####\r\n")];

	/* TODO support other cert types as well */
	if (cert_type != 0) {
		return -EINVAL;
	}

	if (!sock) {
		return -EINVAL;
	}

	__ASSERT_NO_MSG(cert_len <= MDM_MAX_CERT_LENGTH);

	snprintk(send_buf, sizeof(send_buf), "AT+USECMNG=0,%d,\"%s\",%d", cert_type, filename,
		 cert_len);

	k_sem_take(&mdata.cmd_handler_data.sem_tx_lock, K_FOREVER);

	ret = modem_cmd_send_nolock(&mctx.iface, &mctx.cmd_handler, NULL, 0U, send_buf, NULL,
				    K_NO_WAIT);
	if (ret < 0) {
		goto exit;
	}

	/* set command handlers */
	ret = modem_cmd_handler_update_cmds(&mdata.cmd_handler_data, handler_cmds, handler_cmds_len,
					    true);
	if (ret < 0) {
		goto exit;
	}

	/* slight pause per spec so that @ prompt is received */
	k_sleep(MDM_PROMPT_CMD_DELAY);
	mctx.iface.write(&mctx.iface, cert_data, cert_len);

	k_sem_reset(&mdata.sem_response);
	ret = k_sem_take(&mdata.sem_response, K_MSEC(1000));

	if (ret == 0) {
		ret = modem_cmd_handler_get_error(&mdata.cmd_handler_data);
	} else if (ret == -EAGAIN) {
		ret = -ETIMEDOUT;
	}

exit:
	/* unset handler commands and ignore any errors */
	(void)modem_cmd_handler_update_cmds(&mdata.cmd_handler_data, NULL, 0U, false);
	k_sem_give(&mdata.cmd_handler_data.sem_tx_lock);

	return ret;
}
#endif

/*
 * Modem Response Command Handlers
 */

/* Handler: OK */
MODEM_CMD_DEFINE(on_cmd_ok)
{
	modem_cmd_handler_set_error(data, 0);
	k_sem_give(&mdata.sem_response);
	return 0;
}

/* Handler: @ */
MODEM_CMD_DEFINE(on_prompt)
{
	k_sem_give(&mdata.sem_prompt);

	/* A direct cmd should return the number of byte processed.
	 * Therefore, here we always return 1
	 */
	return 1;
}

/* Handler: ERROR */
MODEM_CMD_DEFINE(on_cmd_error)
{
	modem_cmd_handler_set_error(data, -EIO);
	k_sem_give(&mdata.sem_response);
	return 0;
}

/* Handler: +CME Error: <err>[0] */
MODEM_CMD_DEFINE(on_cmd_exterror)
{
	/* TODO: map extended error codes to values */
	modem_cmd_handler_set_error(data, -EIO);
	k_sem_give(&mdata.sem_response);
	return 0;
}

/*>
 * Modem Info Command Handlers
 */

/* Handler: <manufacturer> */
MODEM_CMD_DEFINE(on_cmd_atcmdinfo_manufacturer)
{
	size_t out_len;

	out_len = net_buf_linearize(mdata.mdm_manufacturer, sizeof(mdata.mdm_manufacturer) - 1,
				    data->rx_buf, 0, len);
	mdata.mdm_manufacturer[out_len] = '\0';
	LOG_INF("Manufacturer: %s", log_strdup(mdata.mdm_manufacturer));
	return 0;
}

/* Handler: <model> */
MODEM_CMD_DEFINE(on_cmd_atcmdinfo_model)
{
	size_t out_len;

	out_len = net_buf_linearize(mdata.mdm_model, sizeof(mdata.mdm_model) - 1, data->rx_buf, 0,
				    len);
	mdata.mdm_model[out_len] = '\0';
	LOG_INF("Model: %s", log_strdup(mdata.mdm_model));

#if defined(CONFIG_MODEM_UBLOX_SARA_AUTODETECT_VARIANT)
	/* Set modem type */
	if (strstr(mdata.mdm_model, "R4")) {
		mdata.mdm_variant = MDM_VARIANT_UBLOX_R4;
	} else {
		if (strstr(mdata.mdm_model, "U2")) {
			mdata.mdm_variant = MDM_VARIANT_UBLOX_U2;
		}
	}
	LOG_INF("Variant: %d", mdata.mdm_variant);
#endif

	return 0;
}

/* Handler: <rev> */
MODEM_CMD_DEFINE(on_cmd_atcmdinfo_revision)
{
	size_t out_len;

	out_len = net_buf_linearize(mdata.mdm_revision, sizeof(mdata.mdm_revision) - 1,
				    data->rx_buf, 0, len);
	mdata.mdm_revision[out_len] = '\0';
	LOG_INF("Revision: %s", log_strdup(mdata.mdm_revision));
	return 0;
}

/* Handler: <information> */
MODEM_CMD_DEFINE(on_cmd_atcmdinfo_information)
{
	size_t out_len;

	out_len = net_buf_linearize(mdata.mdm_information, sizeof(mdata.mdm_information) - 1,
				    data->rx_buf, 0, len);
	mdata.mdm_information[out_len] = '\0';
	LOG_INF("Information: %s", log_strdup(mdata.mdm_information));
	return 0;
}

/* Handler: <IMEI> */
MODEM_CMD_DEFINE(on_cmd_atcmdinfo_imei)
{
	size_t out_len;

	out_len =
		net_buf_linearize(mdata.mdm_imei, sizeof(mdata.mdm_imei) - 1, data->rx_buf, 0, len);
	mdata.mdm_imei[out_len] = '\0';
	LOG_INF("IMEI: %s", log_strdup(mdata.mdm_imei));
	return 0;
}

/* Handler: <IMSI> */
MODEM_CMD_DEFINE(on_cmd_atcmdinfo_imsi)
{
	size_t out_len;

	out_len =
		net_buf_linearize(mdata.mdm_imsi, sizeof(mdata.mdm_imsi) - 1, data->rx_buf, 0, len);
	mdata.mdm_imsi[out_len] = '\0';
	LOG_INF("IMSI: %s", log_strdup(mdata.mdm_imsi));

#if defined(CONFIG_MODEM_UBLOX_SARA_AUTODETECT_APN)
	/* set the APN automatically */
	modem_detect_apn(mdata.mdm_imsi);
#endif

	return 0;
}

/* Handler: <CCID> */
MODEM_CMD_DEFINE(on_cmd_atcmdinfo_ccid)
{
	size_t out_len;

	out_len = net_buf_linearize(mdata.mdm_ccid, sizeof(mdata.mdm_ccid) - 1, data->rx_buf, 7,
				    len); /*offset of 7
 * bytes to discard 'ccid:_' */
	mdata.mdm_ccid[out_len] = '\0';
	LOG_INF("CCID: %s", log_strdup(mdata.mdm_ccid));
	ccid_ready = true;
	return 0;
}

#if !defined(CONFIG_MODEM_UBLOX_SARA_U2)

#if defined(USE_EXTENDED_SIGNAL_QUALITY_CHECK)
/*
 * Handler: +CESQ: <rxlev>[0],<ber>[1],<rscp>[2],<ecn0>[3],<rsrq>[4],<rsrp>[5]
 */
MODEM_CMD_DEFINE(on_cmd_atcmdinfo_rssi_cesq)
{
	int rsrp, rxlev;

	rsrp = ATOI(argv[5], 0, "rsrp");
	rxlev = ATOI(argv[0], 0, "rxlev");
	if (rsrp >= 0 && rsrp <= 97) {
		mctx.data_rssi = -140 + (rsrp - 1);
		LOG_INF("RSRP: %d", mctx.data_rssi);
	} else if (rxlev >= 0 && rxlev <= 63) {
		mctx.data_rssi = -110 + (rxlev - 1);
		LOG_INF("RSSI: %d", mctx.data_rssi);
	} else {
		mctx.data_rssi = -1000;
		LOG_INF("RSRP/RSSI not known");
	}

	return 0;
}
#else
/*
 * Handler: +CSQ: <signal_power(RSSI)>[0],<qual>[1]
 */
MODEM_CMD_DEFINE(on_cmd_atcmdinfo_rssi_csq)
{
	int rssi, signal_qual;

	rssi = ATOI(argv[0], 0, "rssi");
	signal_qual = ATOI(argv[1], 0, "squal");
	if (rssi >= 0 && rssi <= 31) {
		mdata.rssi = rssi;
		mdata.min_rssi = MIN(rssi, mdata.min_rssi);
		mdata.max_rssi = MAX(rssi, mdata.max_rssi);
		LOG_INF("RSSI: %d, signal_quality: %d", mdata.rssi, signal_qual);
		LOG_INF("MIN_RSSI, MAX_RSSI = %d, %d", mdata.min_rssi, mdata.max_rssi);
	}
	return 0;
}
#endif /* USE_EXTENDED_SIGNAL_QUALITY_CHECK */
#endif

#if defined(CONFIG_MODEM_UBLOX_SARA_U2) || defined(CONFIG_MODEM_UBLOX_SARA_AUTODETECT_VARIANT)
/* Handler: +CSQ: <signal_power>[0],<qual>[1] */
MODEM_CMD_DEFINE(on_cmd_atcmdinfo_rssi_csq)
{
	int rssi;

	rssi = ATOI(argv[0], 0, "signal_power");
	if (rssi == 31) {
		mctx.data_rssi = -46;
	} else if (rssi >= 0 && rssi <= 31) {
		/* FIXME: This value depends on the RAT */
		mctx.data_rssi = -110 + ((rssi * 2) + 1);
	} else {
		mctx.data_rssi = -1000;
	}

	LOG_INF("RSSI: %d", mctx.data_rssi);
	return 0;
}
#endif

static int unquoted_atoi(const char *s, int base)
{
	if (*s == '"') {
		s++;
	}

	return strtol(s, NULL, base);
}
#if defined(CONFIG_MODEM_CELL_INFO)

/*
 * Handler: +COPS: <mode>[0],<format>[1],<oper>[2]
 */
MODEM_CMD_DEFINE(on_cmd_atcmdinfo_cops)
{
	if (argc >= 3) {
		mctx.data_operator = unquoted_atoi(argv[2], 10);
		LOG_INF("operator: %u", mctx.data_operator);
	}

	return 0;
}

/*
 * Handler: +CEREG: <n>[0],<stat>[1],<tac>[2],<ci>[3],<AcT>[4]
 */
MODEM_CMD_DEFINE(on_cmd_atcmdinfo_cereg)
{
	if (argc >= 4) {
		mctx.data_lac = unquoted_atoi(argv[2], 16);
		mctx.data_cellid = unquoted_atoi(argv[3], 16);
		LOG_INF("lac: %u, cellid: %u", mctx.data_lac, mctx.data_cellid);
	}

	return 0;
}

static const struct setup_cmd query_cellinfo_cmds[] = {
	SETUP_CMD_NOHANDLE("AT+CEREG=2"),
	SETUP_CMD("AT+CEREG?", "", on_cmd_atcmdinfo_cereg, 5U, ","),
	SETUP_CMD_NOHANDLE("AT+COPS=3,2"),
	SETUP_CMD("AT+COPS?", "", on_cmd_atcmdinfo_cops, 3U, ","),
};
#endif /* CONFIG_MODEM_CELL_INFO */

/*
 * Handler: [+CGDCONT: <cid>,<PDP_type>,
	<APN>,<PDP_addr>,<d_comp>,
	<h_comp>[,<IPv4AddrAlloc>,
	<emergency_indication>[,<P-CSCF_
	discovery>,<IM_CN_Signalling_Flag_
	Ind>[,<NSLPI>]]]]

 we're interested in PDP_addr != 0.0.0.0 to make sure that socket connection is
 allowed.
 */
MODEM_CMD_DEFINE(on_cmd_atcmdinfo_cgdcont)
{
	memcpy(mdata.mdm_pdp_addr, argv[3], 17); //17: "xxx.xxx.xxx.xxx", for
	// the check_ip function, we're only interested in the fact that the
	// first 8 characters are not "0.0.0.0
	/*TODO: add more sofistication in handling the string if needed.*/
	LOG_DBG("IP: %s", mdata.mdm_pdp_addr);
	return 0;
}

MODEM_CMD_DEFINE(on_cmd_atcmdinfo_upsv_get)
{
	LOG_DBG("UPSV? handler, %d", argc);
	LOG_DBG("modem power mode: %s", argv[1]);
	int val = 0;
	char *ptr = argv[1];

	while (*ptr != '\0' && val == 0) {
		val = atoi(ptr);
		ptr++;
	}
	mdata.upsv_state = val;
	LOG_INF("UPSV mode = %d", val);
	if (val != 4 && val != 0) {
		LOG_WRN("Unexpected value for UPSV mode setting!");
	}
	return 0;
}

MODEM_CMD_DEFINE(on_cmd_atcmdinfo_cgact_get)
{
	LOG_DBG("CGACT? handler, %d", argc);
	LOG_DBG("PDP context state: ,%s", argv[1]);
	int val = atoi(argv[1]);

	if (val == 1) {
		mdata.pdp_active = true;
		LOG_INF("PDP context ACTIVATED!");
	} else {
		mdata.pdp_active = false;
		LOG_INF("PDP context DEACTIVATED!");
	}
	return 0;
}

MODEM_CMD_DEFINE(on_cmd_atcmdinfo_cops_get)
{
	LOG_DBG("COPS? handler, %d", argc);
	mdata.session_rat = atoi(argv[3]);
	mdata.mnc = unquoted_atoi(argv[2], 10);
	return 0;
}
/*
 * Modem Socket Command Handlers
 */

/* Handler: +USOCR: <socket_id>[0] */
MODEM_CMD_DEFINE(on_cmd_sockcreate)
{
	struct modem_socket *sock = NULL;

	/* look up new socket by special id */
	sock = modem_socket_from_newid(&mdata.socket_config);
	if (sock) {
		sock->id = ATOI(argv[0], mdata.socket_config.base_socket_num - 1, "socket_id");
		/* on error give up modem socket */
		if (sock->id == mdata.socket_config.base_socket_num - 1) {
			modem_socket_put(&mdata.socket_config, sock->sock_fd);
		} else {
			mdata.last_sock = sock->id;
		}
	}
	/* don't give back semaphore -- OK to follow */
	return 0;
}

/* Handler: +USO[WR|ST]: <socket_id>[0],<length>[1] */
MODEM_CMD_DEFINE(on_cmd_sockwrite)
{
	mdata.sock_written = ATOI(argv[1], 0, "length");
	LOG_DBG("bytes written: %d", mdata.sock_written);
	return 0;
}

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
/* Handler: +USECMNG: 0,<type>[0],<internal_name>[1],<md5_string>[2] */
MODEM_CMD_DEFINE(on_cmd_cert_write)
{
	LOG_DBG("cert md5: %s", log_strdup(argv[2]));
	return 0;
}
#endif

/* Common code for +USOR[D|F]: "<data>" */
static int on_cmd_sockread_common(int socket_id, struct modem_cmd_handler_data *data,
				  int socket_data_length, uint16_t len)
{
	struct modem_socket *sock = NULL;
	struct socket_read_data *sock_data;
	int ret;

	if (!len) {
		LOG_ERR("Short +USOR[D|F] value.  Aborting!");
		return -EAGAIN;
	}

	/*
	 * make sure we still have buf data and next char in the buffer is a
	 * quote.
	 */
	if (!data->rx_buf || *data->rx_buf->data != '\"') {
		LOG_ERR("Incorrect format! Ignoring data!");
		return -EINVAL;
	}

	/* zero length */
	if (socket_data_length < 0) {
		LOG_ERR("Length problem (%d).  Aborting!", socket_data_length);
		return -EAGAIN;
	}

	/* check to make sure we have all of the data (minus quotes) */
	if ((net_buf_frags_len(data->rx_buf) - 2) < socket_data_length) {
		LOG_DBG("Not enough data -- wait!");
		return -EAGAIN;
	}

	/* skip quote */
	len--;
	net_buf_pull_u8(data->rx_buf);
	if (!data->rx_buf->len) {
		data->rx_buf = net_buf_frag_del(NULL, data->rx_buf);
	}

	sock = modem_socket_from_id(&mdata.socket_config, socket_id);
	if (!sock) {
		LOG_ERR("Socket not found! (%d)", socket_id);
		ret = -EINVAL;
		goto exit;
	}

	sock_data = (struct socket_read_data *)sock->data;
	if (!sock_data) {
		LOG_ERR("Socket data not found! Skip handling (%d)", socket_id);
		ret = -EINVAL;
		goto exit;
	}

	ret = net_buf_linearize(sock_data->recv_buf, sock_data->recv_buf_len, data->rx_buf, 0,
				(uint16_t)socket_data_length);
	data->rx_buf = net_buf_skip(data->rx_buf, ret);
	sock_data->recv_read_len = ret;
	if (ret != socket_data_length) {
		LOG_ERR("Total copied data is different then received data!"
			" copied:%d vs. received:%d",
			ret, socket_data_length);
		ret = -EINVAL;
	}

exit:
	/* remove packet from list (ignore errors) */
	(void)modem_socket_packet_size_update(&mdata.socket_config, sock, -socket_data_length);

	/* don't give back semaphore -- OK to follow */
	return ret;
}

/*
 * Handler: +USORF: <socket_id>[0],<remote_ip_addr>[1],<remote_port>[2],
 *          <length>[3],"<data>"
*/
MODEM_CMD_DEFINE(on_cmd_sockreadfrom)
{
	/* TODO: handle remote_ip_addr */

	return on_cmd_sockread_common(ATOI(argv[0], 0, "socket_id"), data,
				      ATOI(argv[3], 0, "length"), len);
}

/* Handler: +USORD: <socket_id>[0],<length>[1],"<data>" */
MODEM_CMD_DEFINE(on_cmd_sockread)
{
	/*
	 * See https://content.u-blox.com/sites/default/files/SARA-R4_ATCommands_UBX-17003787.pdf
	 * For SARA-R4, if 0 bytes are available, the length is skipped, i.e.
	 * +USORD: <socket_id>[0],""
	 */
	int length;
	if (strncmp("\"\"", argv[1], 2) == 0) {
		length = 0;
	} else {
		length = ATOI(argv[1], 0, "length");
	}
	return on_cmd_sockread_common(ATOI(argv[0], 0, "socket_id"), data, length, len);
}

//#if defined(CONFIG_DNS_RESOLVER)
/* Handler: +UDNSRN: "<resolved_ip_address>"[0], "<resolved_ip_address>"[1] */
MODEM_CMD_DEFINE(on_cmd_dns)
{
	/* chop off end quote */
	argv[0][strlen(argv[0]) - 1] = '\0';

	/* FIXME: Hard-code DNS on SARA-R4 to return IPv4 */
	result_addr.sa_family = AF_INET;
	/* skip beginning quote when parsing */
	(void)net_addr_pton(result.ai_family, &argv[0][1],
			    &((struct sockaddr_in *)&result_addr)->sin_addr);
	return 0;
}
//#endif

#ifdef DEBUG_UFTPC
/* Handler +UFTPER: <error_class>,<error_code> */
MODEM_CMD_DEFINE(on_cmd_uftper)
{
	LOG_DBG("FTP ERROR %s,%s", argv[0], argv[1]);
	return 0;
}
#endif

/*
 * MODEM UNSOLICITED NOTIFICATION HANDLERS
 */

/* Handler: +UUSOCL: <socket_id>[0] */
MODEM_CMD_DEFINE(on_cmd_socknotifyclose)
{
	struct modem_socket *sock;

	sock = modem_socket_from_id(&mdata.socket_config, ATOI(argv[0], 0, "socket_id"));
	if (sock >= 0) {
		sock->is_connected = false;
		/* make sure socket data structure is reset */
		modem_socket_put(&mdata.socket_config, sock->sock_fd);
	}
	return 0;
}

/* Handler: +UUSOR[D|F]: <socket_id>[0],<length>[1] */
MODEM_CMD_DEFINE(on_cmd_socknotifydata)
{
	int ret, socket_id, new_total;
	struct modem_socket *sock;

	socket_id = ATOI(argv[0], 0, "socket_id");
	new_total = ATOI(argv[1], 0, "length");

	sock = modem_socket_from_id(&mdata.socket_config, socket_id);
	if (!sock) {
		return 0;
	}
	ret = modem_socket_packet_size_update(&mdata.socket_config, sock, new_total);
	if (ret < 0) {
		LOG_ERR("socket_id:%d left_bytes:%d err: %d", socket_id, new_total, ret);
	}
	if (new_total > 0) {
		modem_socket_data_ready(&mdata.socket_config, sock);
	}
	return 0;
}

/* Handler: +CREG: <stat>[0] */
MODEM_CMD_DEFINE(on_cmd_socknotifycreg)
{
	mdata.ev_creg = ATOI(argv[0], 0, "stat");
	LOG_DBG("CREG:%d", mdata.ev_creg);
	return 0;
}

/* Handler: +UFWINSTALL: <progress_install>[0] */
MODEM_CMD_DEFINE(on_cmd_ufwinstall_urc)
{
	uint8_t status = STATUS_UPGRADE_RUNNING;
	int mdm_fw_progress = ATOI(argv[0], 0, "progress");
	if (mdm_fw_progress == 128) {
		k_sem_give(&mdata.sem_fw_install);
		status = STATUS_UPGRADE_NOT_RUNNING;
	} else if (mdm_fw_progress > 100) {
		k_sem_reset(&mdata.sem_fw_install);
		status = STATUS_UPGRADE_NOT_RUNNING;
	}
	/* The NVS will handle to not re-write the same value */
	int ret = modem_write_status(status);
	if (ret != 0) {
		LOG_ERR("Failed to write modem install bit to ext flash, error %i ", ret);
		return ret;
	}
	return ret;
}

/* Handler: +UUSOLI: <socket>,<ip_address>,
<port>,<listening_socket>,<local_
ip_address>,<listening_port> */
MODEM_CMD_DEFINE(on_cmd_socknotify_listen)
{
	LOG_DBG("Received new message on listening socket:%s, port:%s", argv[3], argv[5]);
	if (atoi(argv[3]) == 0 && atoi(argv[5]) == CONFIG_NF_LISTENING_PORT) {
		k_sem_give(&listen_sem);
	}
	return 0;
}

MODEM_CMD_DEFINE(on_cmd_socknotify_listen_udp)
{
	LOG_DBG("Received new message on UDP listening socket!");
	k_sem_give(&listen_sem);
	return 0;
}

/* Handler: +UUFTPCR: <op_code>,<ftp_result>[,<md5_sum>]  */

MODEM_CMD_DEFINE(on_cmd_ftp_urc)
{
	mdata.ftp_op_code = atoi(argv[0]);
	mdata.ftp_result = atoi(argv[1]);
	k_sem_give(&mdata.sem_ftp);
	return 0;
}

/* RX thread */
static void modem_rx(void)
{
	while (true) {
		/* wait for incoming data */
		k_sem_take(&mdata.iface_data.rx_sem, K_FOREVER);

		mctx.cmd_handler.process(&mctx.cmd_handler, &mctx.iface);

		/* give up time if we have a solid stream of data */
		k_yield();
	}
}

bool modem_has_power(void)
{
#if !DT_INST_NODE_HAS_PROP(0, mdm_vint_gpios)
#error "Modem must have VINT pin!"
#endif

	return modem_pin_read(&mctx, MDM_VINT) != 0;
}

static int uart_state_set(enum pm_device_state target_state)
{
	int ret = 0;

	enum pm_device_state current_state;
	ret = pm_device_state_get(MDM_UART_DEV, &current_state);
	if (ret) {
		LOG_ERR("pm_device_state_get: %d", ret);
		return -EIO;
	}

	if (target_state == current_state) {
		return 0;
	}

	ret = pm_device_state_set(MDM_UART_DEV, target_state);
	if (ret) {
		LOG_ERR("pm_device_state_set: %d", ret);
		return -EIO;
	}

	return 0;
}

static bool modem_rx_pin_is_high(void)
{
#if DT_PROP(DT_INST_BUS(0), rx_pin) >= 32
/* This error is used to detect unhandled situations
   The situations are unhandled to avoid unnecessary complexity */
#error "Modem RX pin must be in GPIO0"
#endif

	uint32_t rx_pin = DT_PROP(DT_INST_BUS(0), rx_pin);

	uint32_t values = 0;
	const struct device *gpio_dev = device_get_binding(DT_LABEL(DT_NODELABEL(gpio0)));
	gpio_pin_configure(gpio_dev, rx_pin, GPIO_INPUT);
	gpio_port_get_raw(gpio_dev, &values);

	return ((values & (1 << rx_pin)) != 0);
}

static int pin_init(void)
{
	modem_pin_config(&mctx, MDM_POWER, true);
	LOG_INF("Setting Modem Pins");

#if DT_INST_NODE_HAS_PROP(0, mdm_reset_gpios)
	LOG_DBG("MDM_RESET_PIN -> NOT_ASSERTED");
	modem_pin_write(&mctx, MDM_RESET, MDM_RESET_NOT_ASSERTED);
#endif

	uart_state_set(PM_DEVICE_STATE_SUSPENDED);
	k_sleep(K_MSEC(100));

	LOG_DBG("MDM_POWER_PIN -> ENABLE");
	modem_pin_write(&mctx, MDM_POWER, MDM_POWER_ENABLE);
	k_sleep(K_SECONDS(4));

	if (modem_has_power()) {
		LOG_DBG("MDM_POWER_PIN -> DISABLE");
		modem_pin_write(&mctx, MDM_POWER, MDM_POWER_DISABLE);
#if defined(CONFIG_MODEM_UBLOX_SARA_U2)
		k_sleep(K_SECONDS(1));
#else
		k_sleep(K_SECONDS(4));
#endif
		LOG_DBG("MDM_POWER_PIN -> ENABLE");
		modem_pin_write(&mctx, MDM_POWER, MDM_POWER_ENABLE);
		k_sleep(K_SECONDS(1));

		/* make sure module is powered off */
#if DT_INST_NODE_HAS_PROP(0, mdm_vint_gpios)
		LOG_DBG("Waiting for MDM_VINT_PIN = 0");

		while (modem_has_power()) {
#if defined(CONFIG_MODEM_UBLOX_SARA_U2)
			/* try to power off again */
			LOG_DBG("MDM_POWER_PIN -> DISABLE");
			modem_pin_write(&mctx, MDM_POWER, MDM_POWER_DISABLE);
			k_sleep(K_SECONDS(1));
			LOG_DBG("MDM_POWER_PIN -> ENABLE");
			modem_pin_write(&mctx, MDM_POWER, MDM_POWER_ENABLE);
#endif
			k_sleep(K_MSEC(100));
		}
#else
		k_sleep(K_SECONDS(8));
#endif
	}

	if (!modem_has_power()) {
		LOG_DBG("MDM_POWER_PIN -> DISABLE");

		unsigned int irq_lock_key = irq_lock();

		modem_pin_write(&mctx, MDM_POWER, MDM_POWER_DISABLE);
#if defined(CONFIG_MODEM_UBLOX_SARA_U2)
		k_usleep(50); /* 50-80 microseconds */
#else
		k_sleep(K_SECONDS(1));
#endif
		modem_pin_write(&mctx, MDM_POWER, MDM_POWER_ENABLE);

		irq_unlock(irq_lock_key);

		LOG_DBG("MDM_POWER_PIN -> ENABLE");

#if DT_INST_NODE_HAS_PROP(0, mdm_vint_gpios)
		LOG_DBG("Waiting for MDM_VINT_PIN = 1");
		do {
			k_sleep(K_MSEC(100));
		} while (!modem_has_power());
#else
		k_sleep(K_SECONDS(10));
#endif
	}

	/* Wait for modem GPIO RX pin to rise, indicating readiness */
	LOG_DBG("Waiting for Modem RX = 1");
	do {
		k_sleep(K_MSEC(100));
	} while (!modem_rx_pin_is_high());

	modem_pin_config(&mctx, MDM_POWER, false);

	uart_state_set(PM_DEVICE_STATE_ACTIVE);

	LOG_DBG("Setting increased drive strength for UART TX and RX gpios");
	const struct device *gpio_dev = device_get_binding(DT_LABEL(DT_NODELABEL(gpio0)));

	uint32_t tx_pin = DT_PROP(DT_INST_BUS(0), tx_pin);
	int ret = gpio_pin_configure(gpio_dev, tx_pin,
				     GPIO_ACTIVE_HIGH | GPIO_DS_ALT_HIGH | GPIO_DS_ALT_LOW);
	if (ret != 0) {
		LOG_ERR("Failed to set GPIO parameters for the UART TX pin");
	}

	uint32_t rx_pin = DT_PROP(DT_INST_BUS(0), rx_pin);
	ret = gpio_pin_configure(gpio_dev, rx_pin,
				 GPIO_ACTIVE_HIGH | GPIO_DS_ALT_HIGH | GPIO_DS_ALT_LOW |
					 GPIO_PULL_UP);
	if (ret != 0) {
		LOG_ERR("Failed to set GPIO parameters for the UART RX pin");
	}

	LOG_DBG("... Done!");

	return 0;
}

#if defined(CONFIG_MODEM_UBLOX_SARA_AUTODETECT_VARIANT)
static void modem_rssi_query_work(struct k_work *work)
{
	static const struct modem_cmd cmds[] = {
		MODEM_CMD("+CSQ: ", on_cmd_atcmdinfo_rssi_csq, 2U, ","),
		MODEM_CMD("+CESQ: ", on_cmd_atcmdinfo_rssi_cesq, 6U, ","),
	};
	const char *send_cmd_u2 = "AT+CSQ";
	const char *send_cmd_r4 = "AT+CESQ";
	int ret;

	/* choose cmd according to variant */
	const char *send_cmd = send_cmd_r4;

	if (mdata.mdm_variant == MDM_VARIANT_UBLOX_U2) {
		send_cmd = send_cmd_u2;
	}

	/* query modem RSSI */
	ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, cmds, ARRAY_SIZE(cmds), send_cmd,
			     &mdata.sem_response, MDM_AT_CMD_TIMEOUT);
	if (ret < 0) {
		LOG_ERR("AT+C[E]SQ ret:%d", ret);
	}

#if defined(CONFIG_MODEM_CELL_INFO)
	/* query cell info */
	ret = modem_cmd_handler_setup_cmds_nolock(&mctx.iface, &mctx.cmd_handler,
						  query_cellinfo_cmds,
						  ARRAY_SIZE(query_cellinfo_cmds),
						  &mdata.sem_response, MDM_CMD_TIMEOUT);
	if (ret < 0) {
		LOG_WRN("modem query for cell info returned %d", ret);
	}
#endif

#if defined(CONFIG_MODEM_UBLOX_SARA_RSSI_WORK)
	/* re-start RSSI query work */
	if (work) {
		k_work_reschedule_for_queue(&modem_workq, &mdata.rssi_query_work,
					    K_SECONDS(CONFIG_MODEM_UBLOX_SARA_RSSI_WORK_PERIOD));
	}
#endif
}
#else
static void modem_rssi_query_work(struct k_work *work)
{
#if defined(CONFIG_MODEM_UBLOX_SARA_RSSI_WORK)
	/* re-start RSSI query work */
	if (work) {
		k_work_reschedule_for_queue(&modem_workq, &mdata.rssi_query_work,
					    K_SECONDS(CONFIG_MODEM_UBLOX_SARA_RSSI_WORK_PERIOD));
	}
#endif
	if (mdata.upsv_state == 4 || stop_rssi_work || (mdata.ev_creg != 5 && mdata.ev_creg != 1)) {
		return;
	}
	static const struct modem_cmd cmd =
#if defined(CONFIG_MODEM_UBLOX_SARA_U2)
		MODEM_CMD("+CSQ: ", on_cmd_atcmdinfo_rssi_csq, 2U, ",");
	static char *send_cmd = "AT+CSQ";
#else
		MODEM_CMD("+CSQ: ", on_cmd_atcmdinfo_rssi_csq, 2U, ",");
	static char *send_cmd = "AT+CSQ";
#endif
	int ret;

	/* query modem RSSI */
	ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, &cmd, 1U, send_cmd,
			     &mdata.sem_response, K_SECONDS(1));
	if (ret < 0) {
		LOG_ERR("AT+C[E]SQ ret:%d", ret);
	}

#if defined(CONFIG_MODEM_CELL_INFO)
	/* query cell info */
	ret = modem_cmd_handler_setup_cmds_nolock(&mctx.iface, &mctx.cmd_handler,
						  query_cellinfo_cmds,
						  ARRAY_SIZE(query_cellinfo_cmds),
						  &mdata.sem_response, MDM_CMD_TIMEOUT);
	if (ret < 0) {
		LOG_WRN("modem query for cell info returned %d", ret);
	}
#endif
}
#endif

static int modem_read_status(uint8_t *status)
{
	if (modem_read_status_cb) {
		return modem_read_status_cb(status);
	} else {
		LOG_WRN("modem read status not set");
	}
	return -1;
}

static int modem_write_status(uint8_t status)
{
	if (modem_write_status_cb) {
		return modem_write_status_cb(status);
	} else {
		LOG_WRN("modem write status not set");
	}
	return -1;
}

void set_modem_status_cb(int (*read_status)(uint8_t *), int (*write_status)(uint8_t))
{
	modem_read_status_cb = read_status;
	modem_write_status_cb = write_status;
}

static int modem_reset(void)
{
	LOG_INF("modem_reset");
	int ret = 0, retry_count = 0, counter = 0;
	static uint8_t reset_counter;
	uint8_t is_installing = STATUS_UPGRADE_NOT_RUNNING;
	stop_rssi_work = false;
	mdata.min_rssi = 31;
	mdata.max_rssi = 0;
	/*
	 * In case of reset, be sure that we release all semaphores a socket might
	 * still cling onto. Failing to do so might lead to a dead-lock after a
	 * modem reset.
	 * @see https://github.com/zephyrproject-rtos/zephyr/issues/38632
	 */
	for (int i = 0; i < mdata.socket_config.sockets_len; i++) {
		if (mdata.sockets[i].id < mdata.socket_config.base_socket_num) {
			continue;
		}
		LOG_INF("cleaning up socket %d", mdata.sockets[i].sock_fd);
		modem_socket_put(&mdata.socket_config, mdata.sockets[i].sock_fd);
	}
	memset(mdata.iface_data.rx_rb_buf, 0, mdata.iface_data.rx_rb_buf_len);
	memset(mdata.cmd_handler_data.match_buf, 0, mdata.cmd_handler_data.match_buf_len);
	k_sem_reset(&mdata.sem_response);
	k_sem_reset(&mdata.sem_prompt);

	ret = modem_read_status(&is_installing);
	if (ret != 0) {
		LOG_ERR("Failed to read new collar mode to ext flash, error %i ", ret);
		/* If we have an error, default to modem upgrade not running */
		goto error;
	}

	LOG_INF("Modem install nvm: %d", is_installing);
	if (is_installing == STATUS_UPGRADE_RUNNING) {
		LOG_INF("Modem in middle of upgrade, wait for URC end");
		ret = modem_nf_ftp_fw_install(false);
		if (ret != 0) {
			/* We timed out waiting for a URC, let's deactivate upgrade mode and try to continue */
			ret = modem_write_status(STATUS_UPGRADE_NOT_RUNNING);
			if (ret != 0) {
				LOG_ERR("Failed to read new collar mode to ext flash, error %i ",
					ret);
				goto error;
			}
		}
	}

	static const struct setup_cmd mno_profile_cmds[] = {
		SETUP_CMD_NOHANDLE("AT+UMNOPROF=100"),
		SETUP_CMD_NOHANDLE("AT+CFUN=15"),
	};

	static const struct setup_cmd pre_setup_cmds[] = {
		SETUP_CMD_NOHANDLE("AT+COPS=2"),
		SETUP_CMD_NOHANDLE("AT+URAT=7"),
		SETUP_CMD_NOHANDLE("AT+CPSMS=0"),
		SETUP_CMD_NOHANDLE("AT+UBANDMASK=0,526494,2"),
		//		SETUP_CMD_NOHANDLE("AT+COPS=1,2,\"24201\",0"),
		SETUP_CMD_NOHANDLE("AT+CFUN=15"),
	};

	static const struct setup_cmd setup_cmds0[] = {
		/* stop functionality */
		SETUP_CMD_NOHANDLE("ATE0"),
		SETUP_CMD_NOHANDLE("AT+CRSM=214,28531,0,0,14,"
				   "\"FFFFFFFFFFFFFFFFFFFFFFFFFFFF\""), /*PDP
 * context activation*/
		SETUP_CMD_NOHANDLE("AT+CRSM=214, 28539, 0, 0, 12,"
				   "\"FFFFFFFFFFFFFFFFFFFFFFFF\""), /*FPLMN
// * list*/
	};

	static const struct setup_cmd setup_cmds[] = {
		SETUP_CMD_NOHANDLE("AT+CFUN=0"),
		/* extended error numbers */
		SETUP_CMD_NOHANDLE("AT+CMEE=1"),

#if defined(CONFIG_BOARD_PARTICLE_BORON)
		/* use external SIM */
		SETUP_CMD_NOHANDLE("AT+UGPIOC=23,0,0"),
#endif
#if defined(CONFIG_MODEM_UBLOX_SARA_R4_NET_STATUS_PIN)
		/* enable the network status indication */
		SETUP_CMD_NOHANDLE(
			"AT+UGPIOC=" STRINGIFY(CONFIG_MODEM_UBLOX_SARA_R4_NET_STATUS_PIN) ",2"),
#endif
		/* UNC messages for registration */
		SETUP_CMD_NOHANDLE("AT+CREG=1"),
		/* query modem info */
		SETUP_CMD("AT+CGMI", "", on_cmd_atcmdinfo_manufacturer, 0U, ""),
		SETUP_CMD("AT+CGMM", "", on_cmd_atcmdinfo_model, 0U, ""),
		SETUP_CMD("AT+CGMR", "", on_cmd_atcmdinfo_revision, 0U, ""),
		SETUP_CMD("ATI0", "", on_cmd_atcmdinfo_information, 0U, ""),
		SETUP_CMD("AT+CGSN", "", on_cmd_atcmdinfo_imei, 0U, ""),
		SETUP_CMD("AT+CIMI", "", on_cmd_atcmdinfo_imsi, 0U, ""),
		SETUP_CMD("AT+CCID", "", on_cmd_atcmdinfo_ccid, 0U, ""),
#if !defined(CONFIG_MODEM_UBLOX_SARA_AUTODETECT_APN)
		/* setup PDP context definition */
		SETUP_CMD_NOHANDLE("AT+CGDCONT=1,\"IP\",\"" CONFIG_MODEM_UBLOX_SARA_R4_APN "\""),
		/* start functionality */
		SETUP_CMD_NOHANDLE("AT+CFUN=1"),
		SETUP_CMD_NOHANDLE("ATE0"),
		SETUP_CMD("AT+UPSV=0", "", on_cmd_atcmdinfo_upsv_get, 2U, " "),
		SETUP_CMD("AT+UPSV?", "", on_cmd_atcmdinfo_upsv_get, 2U, " "),
#endif
	};

#if defined(CONFIG_MODEM_UBLOX_SARA_AUTODETECT_VARIANT)
	static const struct setup_cmd post_setup_cmds_u2[] = {
#if !defined(CONFIG_MODEM_UBLOX_SARA_AUTODETECT_APN)
		/* set the APN */
		SETUP_CMD_NOHANDLE("AT+UPSD=0,1,\"" CONFIG_MODEM_UBLOX_SARA_R4_APN "\""),
#endif
		/* set dynamic IP */
		SETUP_CMD_NOHANDLE("AT+UPSD=0,7,\"0.0.0.0\""),
		/* activate the GPRS connection */
		SETUP_CMD_NOHANDLE("AT+UPSDA=0,3"),
	};
#endif

	static const struct setup_cmd post_setup_cmds[] = {
#if defined(CONFIG_MODEM_UBLOX_SARA_U2)
		/* set the APN */
		SETUP_CMD_NOHANDLE("AT+UPSD=0,1,\"" CONFIG_MODEM_UBLOX_SARA_R4_APN "\""),
		/* set dynamic IP */
		SETUP_CMD_NOHANDLE("AT+UPSD=0,7,\"0.0.0.0\""),
		/* activate the GPRS connection */
		SETUP_CMD_NOHANDLE("AT+UPSDA=0,3"),
#else
		SETUP_CMD_NOHANDLE("AT+URAT?"),
		SETUP_CMD("AT+COPS?", "", on_cmd_atcmdinfo_cops_get, 4U, ","),
#endif
	};

restart:

#if defined(CONFIG_MODEM_UBLOX_SARA_AUTODETECT_APN)
	mdata.mdm_apn[0] = '\0';
	strncat(mdata.mdm_apn, CONFIG_MODEM_UBLOX_SARA_R4_APN, sizeof(mdata.mdm_apn) - 1);
#endif

#if defined(CONFIG_MODEM_UBLOX_SARA_RSSI_WORK)
	/* stop RSSI delay work */
	k_work_cancel_delayable(&mdata.rssi_query_work);
#endif

	ret = pin_init();
	if (ret != 0) {
		LOG_ERR("Pin configuration failed.");
		goto error;
	}

	LOG_INF("Waiting for modem to respond");

	/* Give the modem a while to start responding to simple 'AT' commands.
	 * Also wait for CSPS=1 or RRCSTATE=1 notification
	 */

	ret = wake_up();
	if (ret != 0) {
		goto error;
	}

	k_sleep(K_MSEC(50));
	ret = modem_cmd_handler_setup_cmds(&mctx.iface, &mctx.cmd_handler, mno_profile_cmds,
					   ARRAY_SIZE(mno_profile_cmds), &mdata.sem_response,
					   MDM_REGISTRATION_TIMEOUT);
	ret = wake_up();
	if (ret != 0) {
		goto error;
	}

	k_sleep(K_MSEC(50));
	ret = modem_cmd_handler_setup_cmds(&mctx.iface, &mctx.cmd_handler, pre_setup_cmds,
					   ARRAY_SIZE(pre_setup_cmds), &mdata.sem_response,
					   MDM_REGISTRATION_TIMEOUT);

	ret = wake_up();
	if (ret != 0) {
		goto error;
	}

	k_sleep(K_MSEC(250));
	int len = reset_counter++ % 8 == 0 ? ARRAY_SIZE(setup_cmds0) : 1;
	ret = modem_cmd_handler_setup_cmds(&mctx.iface, &mctx.cmd_handler, setup_cmds0, len,
					   &mdata.sem_response, MDM_REGISTRATION_TIMEOUT);
	k_sleep(K_MSEC(250));

	ret = modem_cmd_handler_setup_cmds(&mctx.iface, &mctx.cmd_handler, setup_cmds,
					   ARRAY_SIZE(setup_cmds), &mdata.sem_response,
					   MDM_CMD_TIMEOUT);
	if (ret < 0) {
		goto error;
	}
	k_sleep(K_MSEC(250));

#if defined(CONFIG_MODEM_UBLOX_SARA_AUTODETECT_APN)
	/* autodetect APN from IMSI */
	char cmd[sizeof("AT+CGDCONT=1,\"IP\",\"\"") + MDM_APN_LENGTH];

	snprintk(cmd, sizeof(cmd), "AT+CGDCONT=1,\"IP\",\"%s\"", mdata.mdm_apn);

	/* setup PDP context definition */
	ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0, (const char *)cmd,
			     &mdata.sem_response, MDM_REGISTRATION_TIMEOUT);

	ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0, "AT+CFUN=1",
			     &mdata.sem_response, MDM_REGISTRATION_TIMEOUT);
#endif

	if (strlen(CONFIG_MODEM_UBLOX_SARA_R4_MANUAL_MCCMNO) > 0) {
		/* use manual MCC/MNO entry */
		ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0,
				     "AT+COPS=1,2,\"" CONFIG_MODEM_UBLOX_SARA_R4_MANUAL_MCCMNO "\"",
				     &mdata.sem_response, MDM_REGISTRATION_TIMEOUT);
	} else {
		/* register operator automatically */
		ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0, "AT+COPS=0,2",
				     &mdata.sem_response, MDM_REGISTRATION_TIMEOUT);
	}

	if (ret < 0) {
		LOG_ERR("AT+COPS ret:%d", ret);
		goto error;
	}
	k_sleep(K_MSEC(50));
	LOG_INF("Waiting for network");
	ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0, "AT+CFUN=0",
			     &mdata.sem_response, MDM_CMD_TIMEOUT);
	k_sleep(K_MSEC(500));
	/* Enable RF */
	ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0, "AT+CFUN=1",
			     &mdata.sem_response, MDM_CMD_TIMEOUT);
	ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0, "ATE0", &mdata.sem_response,
			     MDM_CMD_TIMEOUT);

	/*
	 * TODO: A lot of this should be setup as a 3GPP module to handle
	 * basic connection to the network commands / polling
	 */

	/* wait for +CREG: 1(normal) or 5(roaming) */
	counter = 0;
	while (counter++ < 100 && mdata.ev_creg != 1 && mdata.ev_creg != 5) {
		if (counter == 50) {
			LOG_WRN("Force restart of RF functionality");

			/* Disable RF temporarily */
			ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0, "AT+CFUN=0",
					     &mdata.sem_response, MDM_CMD_TIMEOUT);

			k_sleep(K_SECONDS(1));

			/* Enable RF */
			ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0, "AT+CFUN=1",
					     &mdata.sem_response, MDM_CMD_TIMEOUT);
		}

		k_sleep(K_SECONDS(1));
	}
	ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0, "ATE0", &mdata.sem_response,
			     MDM_CMD_TIMEOUT);
	/* query modem RSSI */
	modem_rssi_query_work(NULL);
	k_sleep(MDM_WAIT_FOR_RSSI_DELAY);

	counter = 0;
	/* wait for RSSI < 31 and > 0 */
	while (counter++ < MDM_WAIT_FOR_RSSI_COUNT && (mctx.data_rssi < 0 || mctx.data_rssi > 31)) {
		modem_rssi_query_work(NULL);
		k_sleep(MDM_WAIT_FOR_RSSI_DELAY);
	}

	if (mctx.data_rssi < 0 || mctx.data_rssi > 31) {
		retry_count++;
		if (retry_count >= MDM_NETWORK_RETRY_COUNT) {
			LOG_ERR("Failed network init.  Too many attempts!");
			ret = -ENETUNREACH;
			goto error;
		}

		LOG_ERR("Failed network init.  Restarting process.");
		goto restart;
	}

#if defined(CONFIG_MODEM_UBLOX_SARA_AUTODETECT_VARIANT)
	if (mdata.mdm_variant == MDM_VARIANT_UBLOX_U2) {
#if defined(CONFIG_MODEM_UBLOX_SARA_AUTODETECT_APN)
		/* setup PDP context definition */
		char cmd[sizeof("AT+UPSD=0,1,\"%s\"") + MDM_APN_LENGTH];

		snprintk(cmd, sizeof(cmd), "AT+UPSD=0,1,\"%s\"", mdata.mdm_apn);
		ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0, (const char *)cmd,
				     &mdata.sem_response, MDM_REGISTRATION_TIMEOUT);
#endif
		ret = modem_cmd_handler_setup_cmds(&mctx.iface, &mctx.cmd_handler,
						   post_setup_cmds_u2,
						   ARRAY_SIZE(post_setup_cmds_u2),
						   &mdata.sem_response, MDM_REGISTRATION_TIMEOUT);
	} else {
#endif
		ret = modem_cmd_handler_setup_cmds(&mctx.iface, &mctx.cmd_handler, post_setup_cmds,
						   ARRAY_SIZE(post_setup_cmds), &mdata.sem_response,
						   MDM_REGISTRATION_TIMEOUT);
#if defined(CONFIG_MODEM_UBLOX_SARA_AUTODETECT_VARIANT)
	}
#endif
	if (ret < 0) {
		goto error;
	}

	k_sleep(K_MSEC(50));

	LOG_INF("Network is ready.");
	k_sleep(K_MSEC(250));

	/* The following commmands are necessary for the UFTPC to connect */
	static const struct setup_cmd check_pdp[] = {
		SETUP_CMD_NOHANDLE("AT+UPSD=0,0,0"), SETUP_CMD_NOHANDLE("AT+UPSD=0,100,1"),
		SETUP_CMD_NOHANDLE("AT+CGACT=1,1"),
		SETUP_CMD("AT+CGACT?", "", on_cmd_atcmdinfo_cgact_get, 2U, ",")
	};
	ret = modem_cmd_handler_setup_cmds(&mctx.iface, &mctx.cmd_handler, check_pdp,
					   ARRAY_SIZE(check_pdp), &mdata.sem_response,
					   MDM_REGISTRATION_TIMEOUT);

	if (!mdata.pdp_active) {
		k_sleep(K_MSEC(50));
		ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0, "AT+CGACT=1,1",
				     &mdata.sem_response, MDM_REGISTRATION_TIMEOUT);
		if (ret != 0) {
			LOG_ERR("Problem activating PDP context!");
			return ret;
		}
	}

	k_sleep(K_MSEC(250));

#if defined(CONFIG_MODEM_UBLOX_SARA_RSSI_WORK)
	/* start RSSI query */
	k_work_reschedule_for_queue(&modem_workq, &mdata.rssi_query_work,
				    K_SECONDS(CONFIG_MODEM_UBLOX_SARA_RSSI_WORK_PERIOD));
#endif

error:
	return ret;
}

/*
 * generic socket creation function
 * which can be called in bind() or connect()
 */
static int create_socket(struct modem_socket *sock, const struct sockaddr *addr)
{
	int ret;
	static const struct modem_cmd cmd = MODEM_CMD("+USOCR: ", on_cmd_sockcreate, 1U, "");
	char buf[sizeof("AT+USOCR=#,#####\r")];
	uint16_t local_port = 0U, proto = 6U;

	if (addr) {
		if (addr->sa_family == AF_INET6) {
			local_port = ntohs(net_sin6(addr)->sin6_port);
		} else if (addr->sa_family == AF_INET) {
			local_port = ntohs(net_sin(addr)->sin_port);
		}
	}

	if (sock->ip_proto == IPPROTO_UDP) {
		proto = 17U;
	}

	if (local_port > 0U) {
		snprintk(buf, sizeof(buf), "AT+USOCR=%d,%u", proto, local_port);
	} else {
		snprintk(buf, sizeof(buf), "AT+USOCR=%d", proto);
	}
	/* create socket */
	ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, &cmd, 1U, buf, &mdata.sem_response,
			     MDM_REGISTRATION_TIMEOUT);
	if (ret < 0) {
		goto error;
	}

	char buf2[sizeof("AT+USOSO=#,65535,128,1,####\r")];

	snprintk(buf2, sizeof(buf2), "AT+USOSO=%d,65535,128,1,%d", mdata.last_sock,
		 3000); //TODO: use a config flag for linger time

	k_sleep(K_MSEC(50));

	ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0U, buf2, &mdata.sem_response,
			     MDM_CMD_TIMEOUT);
	if (ret < 0) {
		goto error;
	}

	if (sock->ip_proto == IPPROTO_TLS_1_2) {
		char buf[sizeof("AT+USECPRF=#,#,#######\r")];

		/* Enable socket security */
		snprintk(buf, sizeof(buf), "AT+USOSEC=%d,1,%d", sock->id, sock->id);
		ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0U, buf,
				     &mdata.sem_response, MDM_CMD_TIMEOUT);
		if (ret < 0) {
			goto error;
		}
		/* Reset the security profile */
		snprintk(buf, sizeof(buf), "AT+USECPRF=%d", sock->id);
		ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0U, buf,
				     &mdata.sem_response, MDM_CMD_TIMEOUT);
		if (ret < 0) {
			goto error;
		}
		/* Validate server cert against the CA.  */
		snprintk(buf, sizeof(buf), "AT+USECPRF=%d,0,1", sock->id);
		ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0U, buf,
				     &mdata.sem_response, MDM_CMD_TIMEOUT);
		if (ret < 0) {
			goto error;
		}
		/* Use TLSv1.2 only */
		snprintk(buf, sizeof(buf), "AT+USECPRF=%d,1,3", sock->id);
		ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0U, buf,
				     &mdata.sem_response, MDM_CMD_TIMEOUT);
		if (ret < 0) {
			goto error;
		}
		/* Set root CA filename */
		snprintk(buf, sizeof(buf), "AT+USECPRF=%d,3,\"ca\"", sock->id);
		ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0U, buf,
				     &mdata.sem_response, MDM_CMD_TIMEOUT);
		if (ret < 0) {
			goto error;
		}
	}

	errno = 0;
	return 0;

error:
	LOG_ERR("%s ret:%d", log_strdup(buf), ret);
	modem_socket_put(&mdata.socket_config, sock->sock_fd);
	errno = -ret;
	return -1;
}

/*
 * Socket Offload OPS
 */

static const struct socket_op_vtable offload_socket_fd_op_vtable;

static int offload_socket(int family, int type, int proto)
{
	int ret;

	/* defer modem's socket create call to bind() */
	ret = modem_socket_get(&mdata.socket_config, family, type, proto);
	if (ret < 0) {
		errno = -ret;
		return -1;
	}

	errno = 0;
	return ret;
}

static int offload_close(void *obj)
{
	struct modem_socket *sock = (struct modem_socket *)obj;
	char buf[sizeof("AT+USOCL=#\r")];
	int ret = -1;

	/* make sure we assigned an id */
	if (sock->id < mdata.socket_config.base_socket_num) {
		return 0;
	}

	if (sock->is_connected || sock->ip_proto == IPPROTO_UDP) {
		snprintk(buf, sizeof(buf), "AT+USOCL=%d", sock->id);

		ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0U, buf,
				     &mdata.sem_response,
				     MDM_CMD_USOCL_TIMEOUT); //use a 30 second
		// timeout for the socket close command, as it might take
		// more time compared to other commands.
		if (ret < 0) {
			LOG_ERR("%s ret:%d", log_strdup(buf), ret);
			return ret;
		}
	}

	modem_socket_put(&mdata.socket_config, sock->sock_fd);
	return 0;
}

static int offload_bind(void *obj, const struct sockaddr *addr, socklen_t addrlen)
{
	struct modem_socket *sock = (struct modem_socket *)obj;

	/* save bind address information */
	memcpy(&sock->src, addr, sizeof(*addr));

	/* make sure we've created the socket */
	if (sock->id == mdata.socket_config.sockets_len + 1) {
		if (create_socket(sock, addr) < 0) {
			return -1;
		}
	}

	return 0;
}

static int offload_connect(void *obj, const struct sockaddr *addr, socklen_t addrlen)
{
	struct modem_socket *sock = (struct modem_socket *)obj;
	int ret;
	char buf[sizeof("AT+USOCO=#,!###.###.###.###!,#####,#\r")];
	uint16_t dst_port = 0U;

	if (!addr) {
		errno = EINVAL;
		return -1;
	}

	if (sock->id < mdata.socket_config.base_socket_num - 1) {
		LOG_ERR("Invalid socket_id(%d) from fd:%d", sock->id, sock->sock_fd);
		errno = EINVAL;
		return -1;
	}

	/* make sure we've created the socket */
	if (sock->id == mdata.socket_config.sockets_len + 1) {
		if (create_socket(sock, NULL) < 0) {
			return -1;
		}
	}

	memcpy(&sock->dst, addr, sizeof(*addr));
	if (addr->sa_family == AF_INET6) {
		dst_port = ntohs(net_sin6(addr)->sin6_port);
	} else if (addr->sa_family == AF_INET) {
		dst_port = ntohs(net_sin(addr)->sin_port);
	} else {
		errno = EAFNOSUPPORT;
		return -1;
	}

	/* skip socket connect if UDP */
	if (sock->ip_proto == IPPROTO_UDP) {
		errno = 0;
		return 0;
	}

	snprintk(buf, sizeof(buf), "AT+USOCO=%d,\"%s\",%d", sock->id,
		 modem_context_sprint_ip_addr(addr), dst_port);
	ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0U, buf, &mdata.sem_response,
			     MDM_CMD_CONN_TIMEOUT);
	if (ret < 0) {
		LOG_ERR("%s ret:%d", log_strdup(buf), ret);
		errno = -ret;
		return -1;
	}

	sock->is_connected = true;
	errno = 0;
	return 0;
}

static int offload_listen(void *obj, int backlog)
{
	struct modem_socket *sock = (struct modem_socket *)obj;
	int ret;
	char buf[sizeof("AT+USOLI=#,#####\r")];
	LOG_DBG("Starting listen offload!");

	if (sock->id < mdata.socket_config.base_socket_num - 1) {
		LOG_ERR("Invalid socket_id(%d) from fd:%d", sock->id, sock->sock_fd);
		errno = EINVAL;
		return -1;
	}

	/* make sure we've created the socket */
	if (sock->id == mdata.socket_config.sockets_len + 1) {
		if (create_socket(sock, NULL) < 0) {
			return -1;
		}
	}

	snprintk(buf, sizeof(buf), "AT+USOLI=%d,%d", sock->id, CONFIG_NF_LISTENING_PORT);
	ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0U, buf, &mdata.sem_response,
			     MDM_CMD_CONN_TIMEOUT);
	if (ret < 0) {
		LOG_ERR("%s ret:%d", log_strdup(buf), ret);
		errno = -ret;
		return -1;
	}

	sock->is_connected = true;
	errno = 0;
	return 0;
}

/* support for POLLIN only for now. */
static int offload_poll(struct zsock_pollfd *fds, int nfds, int msecs)
{
	int i;
	void *obj;

	/* Only accept modem sockets. */
	for (i = 0; i < nfds; i++) {
		if (fds[i].fd < 0) {
			continue;
		}

		/* If vtable matches, then it's modem socket. */
		obj = z_get_fd_obj(fds[i].fd,
				   (const struct fd_op_vtable *)&offload_socket_fd_op_vtable,
				   EINVAL);
		if (obj == NULL) {
			return -1;
		}
	}

	return modem_socket_poll(&mdata.socket_config, fds, nfds, msecs);
}

static ssize_t offload_recvfrom(void *obj, void *buf, size_t len, int flags, struct sockaddr *from,
				socklen_t *fromlen)
{
	struct modem_socket *sock = (struct modem_socket *)obj;
	int ret, next_packet_size;
	int wait_ret = 0;
	static const struct modem_cmd cmd[] = {
		MODEM_CMD("+USORF: ", on_cmd_sockreadfrom, 4U, ","),
		MODEM_CMD("+USORD: ", on_cmd_sockread, 2U, ","),
	};
	char sendbuf[sizeof("AT+USORF=#,#####\r")];
	struct socket_read_data sock_data;

	if (!buf || len == 0) {
		errno = EINVAL;
		return -1;
	}

	if (flags & ZSOCK_MSG_PEEK) {
		errno = ENOTSUP;
		return -1;
	}

	next_packet_size = modem_socket_next_packet_size(&mdata.socket_config, sock);
	if (!next_packet_size) {
		if (flags & ZSOCK_MSG_DONTWAIT) {
			errno = EAGAIN;
			return -1;
		}

		if (!sock->is_connected && sock->ip_proto != IPPROTO_UDP) {
			errno = 0;
			return 0;
		}

		wait_ret =
			modem_socket_wait_data_timeout(&mdata.socket_config, sock, K_SECONDS(30));
		if (wait_ret != 0) {
			LOG_WRN("waiting for +UUSORD timed out, tries to read anyway");
			next_packet_size = MDM_MAX_DATA_LENGTH;
		} else {
			next_packet_size =
				modem_socket_next_packet_size(&mdata.socket_config, sock);
		}
	}

	/*
	 * Binary and ASCII mode allows sending MDM_MAX_DATA_LENGTH bytes to
	 * the socket in one command
	 */
	if (next_packet_size > MDM_MAX_DATA_LENGTH) {
		next_packet_size = MDM_MAX_DATA_LENGTH;
	}

	snprintk(sendbuf, sizeof(sendbuf), "AT+USORD=%d,%zd", sock->id,
		 len < next_packet_size ? len : next_packet_size);

	/* socket read settings */
	(void)memset(&sock_data, 0, sizeof(sock_data));
	sock_data.recv_buf = buf;
	sock_data.recv_buf_len = len;
	sock_data.recv_addr = from;
	sock->data = &sock_data;

	ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, cmd, ARRAY_SIZE(cmd), sendbuf,
			     &mdata.sem_response, MDM_CMD_TIMEOUT);
	if (ret < 0) {
		errno = -ret;
		ret = -1;
		goto exit;
	}
	/* If we got time-out waiting for the semaphore and read returned 0 bytes, bail out */
	if (sock_data.recv_read_len == 0 && wait_ret != 0) {
		errno = ETIMEDOUT;
		ret = -1;
		goto exit;
	}

	/* HACK: use dst address as from */
	if (from && fromlen) {
		*fromlen = sizeof(sock->dst);
		memcpy(from, &sock->dst, *fromlen);
	}

	/* return length of received data */
	errno = 0;
	ret = sock_data.recv_read_len;

exit:
	/* clear socket data */
	sock->data = NULL;
	return ret;
}

static ssize_t offload_sendto(void *obj, const void *buf, size_t len, int flags,
			      const struct sockaddr *to, socklen_t tolen)
{
	struct iovec msg_iov = {
		.iov_base = (void *)buf,
		.iov_len = len,
	};
	struct msghdr msg = {
		.msg_iovlen = 1,
		.msg_name = (struct sockaddr *)to,
		.msg_namelen = tolen,
		.msg_iov = &msg_iov,
	};

	int ret = send_socket_data(obj, &msg, MDM_CMD_TIMEOUT);
	if (ret < 0) {
		errno = -ret;
		return -1;
	}

	errno = 0;
	return ret;
}

static int offload_ioctl(void *obj, unsigned int request, va_list args)
{
	switch (request) {
	case ZFD_IOCTL_POLL_PREPARE:
		return -EXDEV;

	case ZFD_IOCTL_POLL_UPDATE:
		return -EOPNOTSUPP;

	case ZFD_IOCTL_POLL_OFFLOAD: {
		struct zsock_pollfd *fds;
		int nfds;
		int timeout;

		fds = va_arg(args, struct zsock_pollfd *);
		nfds = va_arg(args, int);
		timeout = va_arg(args, int);

		return offload_poll(fds, nfds, timeout);
	}

	case F_GETFL:
		return 0;

	default:
		errno = EINVAL;
		return -1;
	}
}

static ssize_t offload_read(void *obj, void *buffer, size_t count)
{
	return offload_recvfrom(obj, buffer, count, 0, NULL, 0);
}

static ssize_t offload_write(void *obj, const void *buffer, size_t count)
{
	return offload_sendto(obj, buffer, count, 0, NULL, 0);
}

static ssize_t offload_sendmsg(void *obj, const struct msghdr *msg, int flags)
{
	ssize_t sent = 0;
	int bkp_iovec_idx;
	struct iovec bkp_iovec = { 0 };
	struct msghdr crafted_msg = {
		.msg_name = msg->msg_name,
		.msg_namelen = msg->msg_namelen,
	};
	size_t full_len = 0;
	int ret;

	/* Compute the full length to be send and check for invalid values */
	for (int i = 0; i < msg->msg_iovlen; i++) {
		if (!msg->msg_iov[i].iov_base || msg->msg_iov[i].iov_len == 0) {
			errno = EINVAL;
			return -1;
		}
		full_len += msg->msg_iov[i].iov_len;
	}

	LOG_DBG("msg_iovlen:%zd flags:%d, full_len:%zd", msg->msg_iovlen, flags, full_len);

	while (full_len > sent) {
		int removed = 0;
		int i = 0;

		crafted_msg.msg_iovlen = msg->msg_iovlen;
		crafted_msg.msg_iov = &msg->msg_iov[0];

		bkp_iovec_idx = -1;
		/*  Iterate on iovec to remove the bytes already sent */
		while (removed < sent) {
			int to_removed = sent - removed;

			if (to_removed >= msg->msg_iov[i].iov_len) {
				crafted_msg.msg_iovlen -= 1;
				crafted_msg.msg_iov = &msg->msg_iov[i + 1];

				removed += msg->msg_iov[i].iov_len;
			} else {
				/* Backup msg->msg_iov[i] before "removing"
				 * starting bytes already send.
				 */
				bkp_iovec_idx = i;
				bkp_iovec.iov_len = msg->msg_iov[i].iov_len;
				bkp_iovec.iov_base = msg->msg_iov[i].iov_base;

				/* Update msg->msg_iov[i] to "remove"
				 * starting bytes already send.
				 */
				msg->msg_iov[i].iov_len -= to_removed;
				msg->msg_iov[i].iov_base =
					&(((uint8_t *)msg->msg_iov[i].iov_base)[to_removed]);

				removed += to_removed;
			}

			i++;
		}

		ret = send_socket_data(obj, &crafted_msg, MDM_CMD_TIMEOUT);

		/* Restore backup iovec when necessary */
		if (bkp_iovec_idx != -1) {
			msg->msg_iov[bkp_iovec_idx].iov_len = bkp_iovec.iov_len;
			msg->msg_iov[bkp_iovec_idx].iov_base = bkp_iovec.iov_base;
		}

		/* Handle send_socket_data() returned value */
		if (ret < 0) {
			errno = -ret;
			return -1;
		}

		sent += ret;
	}

	return (ssize_t)sent;
}

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
static int map_credentials(struct modem_socket *sock, const void *optval, socklen_t optlen)
{
	sec_tag_t *sec_tags = (sec_tag_t *)optval;
	int ret = 0;
	int tags_len;
	sec_tag_t tag;
	int id;
	int i;
	struct tls_credential *cert;

	if ((optlen % sizeof(sec_tag_t)) != 0 || (optlen == 0)) {
		return -EINVAL;
	}

	tags_len = optlen / sizeof(sec_tag_t);
	/* For each tag, retrieve the credentials value and type: */
	for (i = 0; i < tags_len; i++) {
		tag = sec_tags[i];
		cert = credential_next_get(tag, NULL);
		while (cert != NULL) {
			switch (cert->type) {
			case TLS_CREDENTIAL_CA_CERTIFICATE:
				id = 0;
				break;
			case TLS_CREDENTIAL_NONE:
			case TLS_CREDENTIAL_PSK:
			case TLS_CREDENTIAL_PSK_ID:
			default:
				/* Not handled */
				return -EINVAL;
			}
			struct modem_cmd cmd[] = {
				MODEM_CMD("+USECMNG: ", on_cmd_cert_write, 3U, ","),
			};
			ret = send_cert(sock, cmd, 1, cert->buf, cert->len, id);
			if (ret < 0) {
				return ret;
			}

			cert = credential_next_get(tag, cert);
		}
	}

	return 0;
}
#else
static int map_credentials(struct modem_socket *sock, const void *optval, socklen_t optlen)
{
	return -EINVAL;
}
#endif

static int offload_setsockopt(void *obj, int level, int optname, const void *optval,
			      socklen_t optlen)
{
	struct modem_socket *sock = (struct modem_socket *)obj;

	int ret;

	if (IS_ENABLED(CONFIG_NET_SOCKETS_SOCKOPT_TLS) && level == SOL_TLS) {
		switch (optname) {
		case TLS_SEC_TAG_LIST:
			ret = map_credentials(sock, optval, optlen);
			break;
		case TLS_HOSTNAME:
			LOG_WRN("TLS_HOSTNAME option is not supported");
			return -EINVAL;
		case TLS_PEER_VERIFY:
			if (*(uint32_t *)optval != TLS_PEER_VERIFY_REQUIRED) {
				LOG_WRN("Disabling peer verification is not supported");
				return -EINVAL;
			}
			ret = 0;
			break;
		default:
			return -EINVAL;
		}
	} else {
		return -EINVAL;
	}

	return ret;
}

static const struct socket_op_vtable offload_socket_fd_op_vtable = {
	.fd_vtable = {
		.read = offload_read,
		.write = offload_write,
		.close = offload_close,
		.ioctl = offload_ioctl,
	},
	.bind = offload_bind,
	.connect = offload_connect,
	.sendto = offload_sendto,
	.recvfrom = offload_recvfrom,
	.listen = offload_listen,
	.accept = NULL,
	.sendmsg = offload_sendmsg,
	.getsockopt = NULL,
	.setsockopt = offload_setsockopt,
};

static bool offload_is_supported(int family, int type, int proto)
{
	/* TODO offloading always enabled for now. */
	return true;
}

NET_SOCKET_REGISTER(ublox_sara_r4, NET_SOCKET_DEFAULT_PRIO, AF_UNSPEC, offload_is_supported,
		    offload_socket);

//#if defined(CONFIG_DNS_RESOLVER)
/* TODO: This is a bare-bones implementation of DNS handling
 * We ignore most of the hints like ai_family, ai_protocol and ai_socktype.
 * Later, we can add additional handling if it makes sense.
 */
static int offload_getaddrinfo(const char *node, const char *service,
			       const struct zsock_addrinfo *hints, struct zsock_addrinfo **res)
{
	static const struct modem_cmd cmd = MODEM_CMD("+UDNSRN: ", on_cmd_dns, 1U, ",");
	uint32_t port = 0U;
	int ret;
	/* DNS command + 128 bytes for domain name parameter */
	char sendbuf[sizeof("AT+UDNSRN=#,'[]'\r") + 128];

	/* init result */
	(void)memset(&result, 0, sizeof(result));
	(void)memset(&result_addr, 0, sizeof(result_addr));
	/* FIXME: Hard-code DNS to return only IPv4 */
	result.ai_family = AF_INET;
	result_addr.sa_family = AF_INET;
	result.ai_addr = &result_addr;
	result.ai_addrlen = sizeof(result_addr);
	result.ai_canonname = result_canonname;
	result_canonname[0] = '\0';

	if (service) {
		port = ATOI(service, 0U, "port");
		if (port < 1 || port > USHRT_MAX) {
			return DNS_EAI_SERVICE;
		}
	}

	if (port > 0U) {
		/* FIXME: DNS is hard-coded to return only IPv4 */
		if (result.ai_family == AF_INET) {
			net_sin(&result_addr)->sin_port = htons(port);
		}
	}

	/* check to see if node is an IP address */
	if (net_addr_pton(result.ai_family, node,
			  &((struct sockaddr_in *)&result_addr)->sin_addr) == 0) {
		*res = &result;
		return 0;
	}

	/* user flagged node as numeric host, but we failed net_addr_pton */
	if (hints && hints->ai_flags & AI_NUMERICHOST) {
		return DNS_EAI_NONAME;
	}

	snprintk(sendbuf, sizeof(sendbuf), "AT+UDNSRN=0,\"%s\"", node);
	ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, &cmd, 1U, sendbuf, &mdata.sem_response,
			     MDM_DNS_TIMEOUT);
	if (ret < 0) {
		return ret;
	}

	LOG_DBG("DNS RESULT: %s",
		log_strdup(net_addr_ntop(result.ai_family, &net_sin(&result_addr)->sin_addr,
					 sendbuf, NET_IPV4_ADDR_LEN)));

	*res = (struct zsock_addrinfo *)&result;
	return 0;
}

static void offload_freeaddrinfo(struct zsock_addrinfo *res)
{
	/* using static result from offload_getaddrinfo() -- no need to free */
	res = NULL;
}

const struct socket_dns_offload offload_dns_ops = {
	.getaddrinfo = offload_getaddrinfo,
	.freeaddrinfo = offload_freeaddrinfo,
};
//#endif

static int net_offload_dummy_get(sa_family_t family, enum net_sock_type type,
				 enum net_ip_protocol ip_proto, struct net_context **context)
{
	LOG_ERR("CONFIG_NET_SOCKETS_OFFLOAD must be enabled for this driver");

	return -ENOTSUP;
}

/* placeholders, until Zepyr IP stack updated to handle a NULL net_offload */
static struct net_offload modem_net_offload = {
	.get = net_offload_dummy_get,
};

#define HASH_MULTIPLIER 37
static uint32_t hash32(char *str, int len)
{
	uint32_t h = 0;
	int i;

	for (i = 0; i < len; ++i) {
		h = (h * HASH_MULTIPLIER) + str[i];
	}

	return h;
}

static inline uint8_t *modem_get_mac(const struct device *dev)
{
	struct modem_data *data = dev->data;
	uint32_t hash_value;

	data->mac_addr[0] = 0x00;
	data->mac_addr[1] = 0x10;

	/* use IMEI for mac_addr */
	hash_value = hash32(mdata.mdm_imei, strlen(mdata.mdm_imei));

	UNALIGNED_PUT(hash_value, (uint32_t *)(data->mac_addr + 2));

	return data->mac_addr;
}

static void modem_net_iface_init(struct net_if *iface)
{
	const struct device *dev = net_if_get_device(iface);
	struct modem_data *data = dev->data;
	/* Direct socket offload used instead of net offload: */
	iface->if_dev->offload = &modem_net_offload;
	net_if_set_link_addr(iface, modem_get_mac(dev), sizeof(data->mac_addr), NET_LINK_ETHERNET);
	data->net_iface = iface;
	//#ifdef CONFIG_DNS_RESOLVER
	socket_offload_dns_register(&offload_dns_ops);
	//#endif
}

static struct net_if_api api_funcs = {
	.init = modem_net_iface_init,
};

static const struct modem_cmd response_cmds[] = {
	MODEM_CMD("OK", on_cmd_ok, 0U, ""), /* 3GPP */
	MODEM_CMD("ERROR", on_cmd_error, 0U, ""), /* 3GPP */
	MODEM_CMD("+CME ERROR: ", on_cmd_exterror, 1U, ""),
	MODEM_CMD_DIRECT("@", on_prompt),
};

static const struct modem_cmd unsol_cmds[] = {
	MODEM_CMD("+UUSOCL: ", on_cmd_socknotifyclose, 1U, ""),
	MODEM_CMD("+UUSORD: ", on_cmd_socknotifydata, 2U, ","),
	MODEM_CMD("+UUSORF: ", on_cmd_socknotifydata, 2U, ","),
	MODEM_CMD("+CREG: ", on_cmd_socknotifycreg, 1U, ""),
	MODEM_CMD("+UUSOLI: ", on_cmd_socknotify_listen, 6U, ","),
	MODEM_CMD("+UUSORF: ", on_cmd_socknotify_listen_udp, 2U, ","),
	MODEM_CMD_ARGS_MAX("+UUFTPCR: ", on_cmd_ftp_urc, 2U, 3U, ","),
	MODEM_CMD("+UUFWINSTALL: ", on_cmd_ufwinstall_urc, 1U, ""),
};

static int modem_init(const struct device *dev)
{
	int ret = 0;

	ARG_UNUSED(dev);

	k_sem_init(&mdata.sem_response, 0, 1);
	k_sem_init(&mdata.sem_prompt, 0, 1);
	k_sem_init(&mdata.sem_ftp, 0, 1);
	k_sem_init(&mdata.sem_fw_install, 0, 1);

#if defined(CONFIG_MODEM_UBLOX_SARA_RSSI_WORK)
	/* initialize the work queue */
	k_work_queue_start(&modem_workq, modem_workq_stack,
			   K_KERNEL_STACK_SIZEOF(modem_workq_stack), K_PRIO_COOP(1), NULL);
#endif

	/* socket config */
	mdata.socket_config.sockets = &mdata.sockets[0];
	mdata.socket_config.sockets_len = ARRAY_SIZE(mdata.sockets);
	mdata.socket_config.base_socket_num = MDM_BASE_SOCKET_NUM;
	ret = modem_socket_init(&mdata.socket_config, &offload_socket_fd_op_vtable);
	if (ret < 0) {
		goto error;
	}

	/* cmd handler */
	mdata.cmd_handler_data.cmds[CMD_RESP] = response_cmds;
	mdata.cmd_handler_data.cmds_len[CMD_RESP] = ARRAY_SIZE(response_cmds);
	mdata.cmd_handler_data.cmds[CMD_UNSOL] = unsol_cmds;
	mdata.cmd_handler_data.cmds_len[CMD_UNSOL] = ARRAY_SIZE(unsol_cmds);
	mdata.cmd_handler_data.match_buf = &mdata.cmd_match_buf[0];
	mdata.cmd_handler_data.match_buf_len = sizeof(mdata.cmd_match_buf);
	mdata.cmd_handler_data.buf_pool = &mdm_recv_pool;
	mdata.cmd_handler_data.alloc_timeout = K_NO_WAIT;
	mdata.cmd_handler_data.eol = "\r";
	ret = modem_cmd_handler_init(&mctx.cmd_handler, &mdata.cmd_handler_data);
	if (ret < 0) {
		goto error;
	}

	/* modem interface */
	mdata.iface_data.hw_flow_control = DT_PROP(MDM_UART_NODE, hw_flow_control);
	mdata.iface_data.rx_rb_buf = &mdata.iface_rb_buf[0];
	mdata.iface_data.rx_rb_buf_len = sizeof(mdata.iface_rb_buf);
	ret = modem_iface_uart_init(&mctx.iface, &mdata.iface_data, MDM_UART_DEV);
	if (ret < 0) {
		goto error;
	}

	/* modem data storage */
	mctx.data_manufacturer = mdata.mdm_manufacturer;
	mctx.data_model = mdata.mdm_model;
	mctx.data_information = mdata.mdm_information;
	mctx.data_revision = mdata.mdm_revision;
	mctx.data_imei = mdata.mdm_imei;

	/* pin setup */
	mctx.pins = modem_pins;
	mctx.pins_len = ARRAY_SIZE(modem_pins);

	mctx.driver_data = &mdata;

	ret = modem_context_register(&mctx);
	if (ret < 0) {
		LOG_ERR("Error registering modem context: %d", ret);
		goto error;
	}

	/* start RX thread */
	k_thread_create(&modem_rx_thread, modem_rx_stack, K_KERNEL_STACK_SIZEOF(modem_rx_stack),
			(k_thread_entry_t)modem_rx, NULL, NULL, NULL, K_PRIO_COOP(7), 0, K_NO_WAIT);

#if defined(CONFIG_MODEM_UBLOX_SARA_RSSI_WORK)
	/* init RSSI query */
	k_work_init_delayable(&mdata.rssi_query_work, modem_rssi_query_work);
#endif

#if defined(CONFIG_MODEM_UBLOX_INIT_RESET)
	modem_reset();
#endif

error:
	return ret;
}

int modem_nf_reset(void)
{
	return modem_reset();
}

int get_pdp_addr(char **ip_addr)
{
	static const struct modem_cmd cmd =
		MODEM_CMD("+CGDCONT: ", on_cmd_atcmdinfo_cgdcont, 7U, ",");

	char buf[sizeof("AT+CGDCONT?\r")];
	snprintk(buf, sizeof(buf), "AT+CGDCONT?");
	int ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, &cmd, 1, buf, &mdata.sem_response,
				 MDM_REGISTRATION_TIMEOUT);

	if (ret == 0) {
		*ip_addr = mdata.mdm_pdp_addr;
		return 0;
	} else {
		return -1;
	}
}

/* Give the modem a while to start responding to simple 'AT' commands.
	 * Also wait for CSPS=1 or RRCSTATE=1 notification
	 */
static int wake_up(void)
{
	LOG_WRN("Waking up modem!");
	int ret = -1;
	uint8_t counter = 0;
	while (counter++ < 50 && ret < 0) {
		k_sleep(K_SECONDS(1));
		ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0, "AT",
				     &mdata.sem_response, MDM_AT_CMD_TIMEOUT);
		if (ret < 0 && ret != -ETIMEDOUT) {
			return ret;
		} else if (ret == 0) {
			break;
		}
	}

	ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0, "ATE0", &mdata.sem_response,
			     MDM_AT_CMD_TIMEOUT);

	if (ret != 0) {
		return ret;
	}

	k_sleep(K_MSEC(50));
	const struct setup_cmd disable_psv[] = {
		SETUP_CMD_NOHANDLE("AT+UPSV=0"),
		SETUP_CMD_NOHANDLE("AT+CPSMS=0"),
		SETUP_CMD("AT+UPSV?", "", on_cmd_atcmdinfo_upsv_get, 2U, " "),
	};
	if (mdata.upsv_state == 4) {
		ret = modem_cmd_handler_setup_cmds(&mctx.iface, &mctx.cmd_handler, disable_psv,
						   ARRAY_SIZE(disable_psv), &mdata.sem_response,
						   MDM_REGISTRATION_TIMEOUT);
	}

	return ret;
}

int wake_up_from_upsv(void)
{
	uart_state_set(PM_DEVICE_STATE_ACTIVE);
	if (mdata.upsv_state == 4) {
		if (k_sem_take(&mdata.cmd_handler_data.sem_tx_lock, K_SECONDS(2)) == 0) {
			modem_pin_config(&mctx, MDM_POWER, true);

			LOG_DBG("MDM_POWER_PIN -> DISABLE");
			modem_pin_write(&mctx, MDM_POWER, MDM_POWER_DISABLE);
			k_sleep(K_MSEC(10)); /* min. value 100 msec (r4 datasheet
 * section 4.2.10 )*/
			modem_pin_write(&mctx, MDM_POWER, MDM_POWER_ENABLE);

			LOG_DBG("MDM_POWER_PIN -> ENABLE");
			modem_pin_config(&mctx, MDM_POWER, false);

			/* Wait for modem GPIO RX pin to rise, indicating readiness */
			LOG_DBG("Waiting for Modem RX = 1");
			do {
				k_sleep(K_MSEC(100));
			} while (!modem_rx_pin_is_high());

			k_sem_give(&mdata.cmd_handler_data.sem_tx_lock);
			goto check_signal_strength;
		}
		return -EAGAIN;
	} else if (mdata.upsv_state == 0) {
		return wake_up();
	} else {
		return -EIO;
	}

check_signal_strength:
	if (wake_up() != 0) {
		return -EIO;
	}
	if (!stop_rssi_work) {
		mdata.rssi = 0;
		mdata.min_rssi = 31;
		mdata.max_rssi = 0;
		uint8_t counter = 0;
		while (counter++ < MDM_WAIT_FOR_RSSI_COUNT) {
			modem_rssi_query_work(NULL);
			k_sleep(K_MSEC(50));
		}
		if (mdata.max_rssi < MDM_MIN_ALLOWED_RSSI) {
			return -EIO;
		}
	}
	return 0;
}

static int sleep(void)
{
	const struct setup_cmd set_psv[] = {
#ifdef CONFIG_USE_CPSMS
		SETUP_CMD_NOHANDLE("AT+CPSMS=1"),
#endif
		SETUP_CMD_NOHANDLE("AT+UPSV=4"),
		SETUP_CMD("AT+UPSV?", "", on_cmd_atcmdinfo_upsv_get, 2U, " "),
	};
	int ret = modem_cmd_handler_setup_cmds(&mctx.iface, &mctx.cmd_handler, set_psv,
					       ARRAY_SIZE(set_psv), &mdata.sem_response,
					       MDM_REGISTRATION_TIMEOUT);

	if (ret == 0) {
		if (mdata.upsv_state == 4) {
			LOG_INF("Modem power mode switched to 4!");
			return 0;
		} else {
			LOG_ERR("Modem power not mode switched to 4!");
			return -1;
		}
	} else {
		LOG_DBG("UPSV=4 returned %d, ", ret);
		/*TODO: change to a relevent error code, to be handled by
		 * caller.*/
		return ret;
	}
}

static int pwr_off(void)
{
	stop_rssi();
	const struct setup_cmd pwr_off_gracefully[] = {
		SETUP_CMD_NOHANDLE("AT+CPWROFF"),
	};
	enum pm_device_state current_state;
	int ret = pm_device_state_get(MDM_UART_DEV, &current_state);
	if (ret != 0) {
		return -EIO;
	}
	if (current_state == PM_DEVICE_STATE_SUSPENDED) {
		return -EALREADY;
	} else {
		wake_up_from_upsv();
		k_sleep(K_MSEC(100));
		int ret =
			modem_cmd_handler_setup_cmds(&mctx.iface, &mctx.cmd_handler,
						     pwr_off_gracefully,
						     ARRAY_SIZE(pwr_off_gracefully),
						     &mdata.sem_response, MDM_PWR_OFF_CMD_TIMEOUT);
		if (ret != 0) {
			LOG_ERR("Modem PWROFF failed!");
			/*TODO: use power line if AT interface is not
			 * available.*/
			uart_state_set(PM_DEVICE_STATE_SUSPENDED);
			return -EIO;
		}
	}
	uart_state_set(PM_DEVICE_STATE_SUSPENDED);
	k_sleep(K_MSEC(100));
	return ret;
}

int modem_nf_wakeup(void)
{
	if (wake_up_from_upsv() != 0) {
		return modem_reset();
	}
	return 0;
}

int modem_nf_sleep(void)
{
	return sleep();
}

int modem_nf_pwr_off(void)
{
	return pwr_off();
}

int get_ccid(char **ccid)
{
	if (ccid_ready) {
		*ccid = mdata.mdm_ccid;
		return 0;
	} else {
		return -ENODATA;
	}
}

int get_gsm_info(struct gsm_info *session_info)
{
	memset(session_info, 0, sizeof(struct gsm_info));
	session_info->rat = mdata.session_rat;
	session_info->mnc = mdata.mnc;
	session_info->rssi = mdata.rssi;
	session_info->min_rssi = MIN(mdata.min_rssi, mdata.max_rssi);
	session_info->max_rssi = mdata.max_rssi;
	if (ccid_ready)
		memcpy(session_info->ccid, mdata.mdm_ccid, sizeof(session_info->ccid));
	return 0;
}

void stop_rssi(void)
{
	stop_rssi_work = true;
}

/**
 * @brief Modem TX test non-signaling
 * 
 */

struct modem_test_tx_status {
	int8_t utest;
	int8_t cfun;
	bool test_mode;
} modem_test_tx_status;

static modem_test_tx_result_t modem_test_tx_result = {
	.success = false,
	.ch = 0,
	.dbm = -100,
	.seq = -1,
	.mod = -1,
	.dur = -1,
};

/* +UTEST: <mode> */
MODEM_CMD_DEFINE(on_cmd_test_tx_utest_mode)
{
	if (argc >= 1) {
		modem_test_tx_status.utest = atoi(argv[0]);
	} else {
		modem_test_tx_status.utest = -1;
	}
	modem_test_tx_status.test_mode =
		(modem_test_tx_status.utest == 1 && modem_test_tx_status.cfun == 5);

	LOG_WRN("MODEM TEST TX: UTEST=%d (test mode = %s)", modem_test_tx_status.utest,
		modem_test_tx_status.test_mode ? "true" : "false");

	return 0;
}

/* +CFUN: <mode> */
MODEM_CMD_DEFINE(on_cmd_test_tx_cfun_mode)
{
	if (argc >= 1) {
		modem_test_tx_status.cfun = atoi(argv[0]);
	} else {
		modem_test_tx_status.cfun = -1;
	}
	modem_test_tx_status.test_mode =
		(modem_test_tx_status.utest == 1 && modem_test_tx_status.cfun == 5);

	LOG_WRN("MODEM TEST TX: CFUN=%d (test mode = %s)", modem_test_tx_status.cfun,
		modem_test_tx_status.test_mode ? "true" : "false");

	return 0;
}

/* +UTEST: <TX_channel>,<power_control_level>,<training_sequence>,<modulation_mode>,<TX_time_interval> */
MODEM_CMD_DEFINE(on_cmd_test_tx_utest_result)
{
	//LOG_WRN("MODEM TEST TX: UTEST RESULT, len: %d, first arg: %s", argc, argv[0]);
	modem_test_tx_result.success = 0;

	if (argc >= 5) {
		modem_test_tx_result.ch = atoi(argv[0]);
		modem_test_tx_result.dbm = atoi(argv[1]);
		modem_test_tx_result.seq = atoi(argv[2]);
		modem_test_tx_result.mod = atoi(argv[3]);
		modem_test_tx_result.dur = atoi(argv[4]);
		modem_test_tx_result.success = 1;
	}

	LOG_WRN("MODEM TEST TX: test success = %s",
		modem_test_tx_result.success ? "true" : "false");

	return 0;
}

/** 
 * @brief copy test result 
 * 
 */
int modem_test_tx_get_result(modem_test_tx_result_t **test_res)
{
	*test_res = &modem_test_tx_result;

	return 0;
}

/** 
 * @brief run tx test 
 * 
 */
int modem_test_tx_run_test(uint32_t tx_ch, int16_t dbm_level, uint16_t test_dur)
{
	LOG_WRN("MODEM TEST TX START");

	int ret = -1;

	modem_test_tx_status.utest = -1;
	modem_test_tx_status.cfun = -1;
	modem_test_tx_status.test_mode = false;
	modem_test_tx_result.success = 0;

	if (dbm_level < -100 || dbm_level > 100) {
		LOG_ERR("MODEM TEST TX: dbm_level out of range: %d dBm, set to -10 dBm", dbm_level);
		dbm_level = -10;
	}
	if (test_dur < 0 || test_dur > 5000) {
		LOG_ERR("MODEM TEST TX: duration out of range: %d ms, set to 1000 ms", test_dur);
		test_dur = 1000;
	}

	// need to hijack the modem somehow
	// dont know what im doing
	memset(mdata.iface_data.rx_rb_buf, 0, mdata.iface_data.rx_rb_buf_len);
	memset(mdata.cmd_handler_data.match_buf, 0, mdata.cmd_handler_data.match_buf_len);
	k_sem_reset(&mdata.sem_response);
	k_sem_reset(&mdata.sem_prompt);

	static const struct device *gpio0_dev;
	gpio0_dev = device_get_binding(DT_LABEL(DT_NODELABEL(gpio0)));
	gpio_pin_configure(gpio0_dev, 2, GPIO_OUTPUT_HIGH);
	k_msleep(1000);
	gpio_pin_configure(gpio0_dev, 2, GPIO_OUTPUT_LOW);

	//if (wake_up() != 0)  {
	//	LOG_ERR("MODEM TEST TX: error waking up modem");
	//}

	static const struct setup_cmd pre_test_cmds[] = {
		SETUP_CMD_NOHANDLE("AT+COPS=2"),
		SETUP_CMD_NOHANDLE("AT+UTEST=1"),
		SETUP_CMD("AT+UTEST?", "+UTEST: ", on_cmd_test_tx_utest_mode, 1U, ""),
		SETUP_CMD("AT+CFUN?", "+CFUN: ", on_cmd_test_tx_cfun_mode, 1U, ""),
	};

	ret = modem_cmd_handler_setup_cmds(&mctx.iface, &mctx.cmd_handler, pre_test_cmds,
					   ARRAY_SIZE(pre_test_cmds), &mdata.sem_response,
					   MDM_REGISTRATION_TIMEOUT);

	if (ret < 0) {
		LOG_ERR("MODEM TEST TX: pre test cmds error: %d", ret);
	}

	if (modem_test_tx_status.test_mode) {
		/* tx signaling AT+UTEST=3,<TX_channel>,<power_control_level>,,,<TX_time_interval> */
		char cmd_buf[sizeof("AT+UTEST=3,###########,######,,,######")];
		snprintk(cmd_buf, sizeof(cmd_buf), "AT+UTEST=3,%d,%d,,,%d", tx_ch, dbm_level,
			 test_dur);

		const struct modem_cmd cmd_utest_tx =
			MODEM_CMD("+UTEST: ", on_cmd_test_tx_utest_result, 5U, ",");
		ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, &cmd_utest_tx, 1U, cmd_buf,
				     &mdata.sem_response, K_SECONDS(10));

		if (ret < 0) {
			LOG_ERR("MODEM TEST TX: error running test cmd '%s': %d", cmd_buf, ret);
		} else {
			LOG_WRN("MODEM TEST TX: running test\n\t\t\t\tcommand: %s\n\t\t\t\tchannel: %d"
				"\n\t\t\t\tpower: %d dBm\n\t\t\t\tduration: %d ms"
				"\n\t\t\t\tcmd res: %d\n\t\t\t\tsuccess: %s",
				log_strdup(cmd_buf), tx_ch, dbm_level, test_dur, ret,
				modem_test_tx_result.success ? "true" : "false");
			LOG_WRN("MODEM TEST TX: test result\n\t\t\t\tchannel: %d\n\t\t\t\tpwr level: %d dBm"
				"\n\t\t\t\tsequence: %d\n\t\t\t\tmodulation: %d\n\t\t\t\tinterval: %d ms",
				modem_test_tx_result.ch, modem_test_tx_result.dbm,
				modem_test_tx_result.seq, modem_test_tx_result.mod,
				modem_test_tx_result.dur);
		}

		k_sleep(K_MSEC(100));

	} else {
		LOG_ERR("MODEM TEST TX: error setting modem in test mode (UTEST=%d, CFUN=%d)",
			modem_test_tx_status.utest, modem_test_tx_status.cfun);
	}

	LOG_WRN("MODEM TEST TX: resetting modem");
	modem_reset();

	LOG_WRN("MODEM TEST TX DONE (%d)", ret);

	return ret;
}

/** 
 * @brief run tx test with defaul values
 * 
 */
int modem_test_tx_run_test_default(void)
{
	uint32_t tx_ch = 119575; // EARFCN 19575 (1747.50 MHz LTE 3)
	int16_t dbm_level = 0; // dBm
	uint16_t test_dur = 1000; // ms

	return modem_test_tx_run_test(tx_ch, dbm_level, test_dur);
}

void enable_rssi(void)
{
	stop_rssi_work = false;
}

int modem_nf_get_model_and_fw_version(const char **model, const char **version)
{
	if (model == NULL || version == NULL) {
		return -EINVAL;
	}
	if (mctx.data_information == NULL || mctx.data_revision == NULL ||
	    *mctx.data_information == '\0' || *mctx.data_revision == '\0') {
		return -ENODATA;
	}
	*model = mctx.data_information;
	*version = mctx.data_revision;
	return 0;
}

#ifdef DEBUG_UFTPC
static void log_uftp_error()
{
	static const struct modem_cmd cmds[] = { MODEM_CMD("+UFTPER: ", on_cmd_uftper, 2U, ",") };
	int ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, cmds, ARRAY_SIZE(cmds),
				 "AT+UFTPER", &mdata.sem_response, MDM_CMD_TIMEOUT);
	if (ret != 0) {
		LOG_ERR("Cannot get FTP error %d", ret);
	}
}
#endif

int modem_nf_ftp_fw_download(const struct modem_nf_uftp_params *ftp_params, const char *filename)
{
	int ret;
	if (ftp_params == NULL || filename == NULL) {
		return -EINVAL;
	}

	char buf_uftp_server[sizeof("AT+UFTP=\"#,###.###.###.###\"")];
	char buf_uftp_user[sizeof("AT+UFTP=#,\"################################\"")];
	char buf_uftp_pass[sizeof("AT+UFTP=#,\"################################\"")];
	char buf_uftpc_100[sizeof("AT+UFTPC=###,\"########################\"")];

	snprintk(buf_uftp_server, sizeof(buf_uftp_server), "AT+UFTP=0,\"%s\"",
		 ftp_params->ftp_server);
	snprintk(buf_uftp_user, sizeof(buf_uftp_user), "AT+UFTP=2,\"%s\"", ftp_params->ftp_user);
	snprintk(buf_uftp_pass, sizeof(buf_uftp_pass), "AT+UFTP=3,\"%s\"",
		 ftp_params->ftp_password);

	/* Setup FTP parameters */
	const struct setup_cmd uftp_commands[] = { SETUP_CMD_NOHANDLE(buf_uftp_server),
						   SETUP_CMD_NOHANDLE(buf_uftp_user),
						   SETUP_CMD_NOHANDLE(buf_uftp_pass) };
	ret = modem_cmd_handler_setup_cmds(&mctx.iface, &mctx.cmd_handler, uftp_commands,
					   ARRAY_SIZE(uftp_commands), &mdata.sem_response,
					   MDM_CMD_TIMEOUT);
	if (ret != 0) {
		LOG_ERR("UFTP setup commands failed %d", ret);
		return ret;
	}
	/* Connect to the FTP server */
	k_sem_reset(&mdata.sem_ftp);
	mdata.ftp_op_code = 0;
	mdata.ftp_result = 0;
	ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0, "AT+UFTPC=1",
			     &mdata.sem_response, MDM_CMD_TIMEOUT);
	if (ret != 0) {
		LOG_ERR("UFTPC connect failed %d", ret);
		return ret;
	}
	/*  Wait for asynchronous feedback on connect */
	ret = k_sem_take(&mdata.sem_ftp, K_SECONDS(60));
	if (ret != 0) {
		LOG_ERR("UFTPC connect timeout %d", ret);
		return -ETIMEDOUT;
	}
	if (mdata.ftp_op_code != 1 || mdata.ftp_result != 1) {
		LOG_ERR("UFTPC connect failed %d,%d", mdata.ftp_op_code, mdata.ftp_result);
#ifdef DEBUG_UFTPC
		log_uftp_error();
#endif
		return -ECONNREFUSED;
	}
	/* Get the file */
	snprintk(buf_uftpc_100, sizeof(buf_uftpc_100), "AT+UFTPC=100,\"%s\"", filename);
	k_sem_reset(&mdata.sem_ftp);
	mdata.ftp_op_code = 0;
	mdata.ftp_result = 0;

	ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0, buf_uftpc_100,
			     &mdata.sem_response, MDM_CMD_TIMEOUT);
	if (ret != 0) {
		LOG_ERR("Could not get %s  %d", filename, ret);
		return ret;
	}
	ret = k_sem_take(&mdata.sem_ftp, ftp_params->download_timeout_sec > 0 ?
						 K_SECONDS(ftp_params->download_timeout_sec) :
						 K_FOREVER);
	if (ret != 0) {
		LOG_ERR("UFTPC GET timeout %d", ret);
		return -ETIMEDOUT;
	}
	if (mdata.ftp_op_code != 100 || mdata.ftp_result != 1) {
		LOG_ERR("UFTPC=100 failed %d,%d", mdata.ftp_op_code, mdata.ftp_result);
		return -EIO;
	}
	return 0;
}

int modem_nf_ftp_fw_install(bool start_install)
{
	int ret = -1;
	const struct setup_cmd ufinstall_cmds[] = {
		SETUP_CMD_NOHANDLE("AT+UFWINSTALL=1,115200,,1"),
		SETUP_CMD_NOHANDLE("AT+UFWINSTALL"),
	};

	if (start_install) {
		ret = modem_cmd_handler_setup_cmds(&mctx.iface, &mctx.cmd_handler, ufinstall_cmds,
						   ARRAY_SIZE(ufinstall_cmds), &mdata.sem_response,
						   MDM_CMD_TIMEOUT);

		if (ret != 0) {
			LOG_ERR("UFWINSTALL failed %d", ret);
			return ret;
		}
	}

	ret = k_sem_take(&mdata.sem_fw_install, K_MINUTES(40));

	if (ret != 0) {
		LOG_ERR("Modem FW upgrade failed %d", ret);
		return ret;
	}
	return ret;
}

NET_DEVICE_DT_INST_OFFLOAD_DEFINE(0, modem_init, NULL, &mdata, NULL,
				  CONFIG_MODEM_UBLOX_SARA_R4_INIT_PRIORITY, &api_funcs,
				  MDM_MAX_DATA_LENGTH);
