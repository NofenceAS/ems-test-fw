#include "cellular_controller.h"
#include "cellular_controller_events.h"
#include "cellular_helpers_header.h"
#include "messaging_module_events.h"
#include <zephyr.h>
#include "error_event.h"
#include <modem_nf.h>
#include "fw_upgrade_events.h"
#include "selftest.h"
#include "pwr_event.h"
#include "stg_config.h"
#include "diagnostics_events.h"
#include "diagnostic_flags.h"

#define RCV_THREAD_STACK CONFIG_RECV_THREAD_STACK_SIZE
#define RCV_PRIORITY CONFIG_RECV_THREAD_PRIORITY
#define SOCKET_POLL_INTERVAL 1
#define SOCK_RECV_TIMEOUT 10
#define MODULE cellular_controller
#define MESSAGING_ACK_TIMEOUT CONFIG_MESSAGING_ACK_TIMEOUT_MSEC

LOG_MODULE_REGISTER(cellular_controller, 4);

K_SEM_DEFINE(messaging_ack, 0, 1);
K_SEM_DEFINE(fota_progress_update, 0, 1);

struct msg2server {
	char *msg;
	size_t len;
};
static void send_tcp_fn(void);
K_THREAD_DEFINE(send_tcp_from_q, CONFIG_SEND_THREAD_STACK_SIZE, send_tcp_fn, NULL, NULL, NULL,
		K_PRIO_COOP(CONFIG_SEND_THREAD_PRIORITY), 0, 0);

K_MSGQ_DEFINE(msgq, sizeof(struct msg2server), 1, 4);

struct k_poll_event events[1] = { K_POLL_EVENT_STATIC_INITIALIZER(
	K_POLL_TYPE_MSGQ_DATA_AVAILABLE, K_POLL_MODE_NOTIFY_ONLY, &msgq, 0) };

char server_address[STG_CONFIG_HOST_PORT_BUF_LEN - 1];
char server_address_tmp[STG_CONFIG_HOST_PORT_BUF_LEN - 1];
static int server_port;
static char server_ip[15];

/* Connection keep-alive thread structures */
K_KERNEL_STACK_DEFINE(keep_alive_stack, CONFIG_CELLULAR_KEEP_ALIVE_STACK_SIZE);
struct k_thread keep_alive_thread;
static struct k_sem connection_state_sem;

int socket_connect(struct data *, struct sockaddr *, socklen_t);
int socket_listen(struct data *);
int socket_receive(const struct data *, char **);
void listen_sock_poll(void);
int8_t lte_init(void);
bool lte_is_ready(void);

K_SEM_DEFINE(listen_sem, 0, 1); /* this semaphore will be given by the modem
 * driver when receiving the UUSOLI urc code. Socket 0 is the listening
 * socket by design, however the 'socket' number returned in the UUSOLI =
 * number of currently opened sockets + 1 */

K_SEM_DEFINE(close_main_socket_sem, 0, 1);

#if defined(CONFIG_DIAGNOSTIC_EMS_FW)
static bool diagnostic_run_cellular_thread = false;
static uint16_t battery_mv = 0;
#endif

static bool waiting_for_msg = false;
static bool modem_is_ready = false;
static bool power_level_ok = false;
static bool fota_in_progress = false;

static bool enable_mdm_fota = false;

APP_DMEM struct configs conf = {
	.ipv4 = {
		.proto = "IPv4",
		.udp.sock = INVALID_SOCK,
		.tcp.sock = INVALID_SOCK,
	},
};

APP_DMEM struct configs conf_listen = {
	.ipv4 = {
		.proto = "IPv4",
		.udp.sock = INVALID_SOCK,
		.tcp.sock = INVALID_SOCK,
	},
};

static int send_tcp_q(char *, size_t);

void receive_tcp(const struct data *);
K_THREAD_DEFINE(recv_tid, RCV_THREAD_STACK, receive_tcp, &conf.ipv4, NULL, NULL,
		K_PRIO_COOP(RCV_PRIORITY), 0, 0);

extern struct k_sem listen_sem;

K_THREAD_DEFINE(listen_recv_tid, RCV_THREAD_STACK, listen_sock_poll, NULL, NULL, NULL,
		K_PRIO_COOP(RCV_PRIORITY), 0, 0);

static APP_BMEM bool connected;
static uint8_t *pMsgIn = NULL;
static float socket_idle_count;
static char *m_ftp_url = NULL;

static void give_up_main_soc(void)
{
	waiting_for_msg = false;
	k_sem_give(&close_main_socket_sem);
	k_sem_give(&connection_state_sem);
}

void receive_tcp(const struct data *sock_data)
{
	int received;
	char *buf = NULL;
	static bool initializing = true;

	while (1) {
		if (connected && waiting_for_msg) {
			received = socket_receive(sock_data, &buf);
			if (received > 0) {
				socket_idle_count = 0;
#if defined(CONFIG_CELLULAR_CONTROLLER_VERBOSE)
				LOG_WRN("received %d bytes!", received);
#endif
				LOG_WRN("will take semaphore!");
				if (initializing ||
				    k_sem_take(&messaging_ack, K_MSEC(MESSAGING_ACK_TIMEOUT)) ==
					    0) {
					initializing = false;
					pMsgIn = (uint8_t *)k_malloc(received);
					memcpy(pMsgIn, buf, received);
					struct cellular_proto_in_event *msgIn =
						new_cellular_proto_in_event();
					msgIn->buf = pMsgIn;
					msgIn->len = received;
					LOG_INF("Submitting msgIn event!");
					EVENT_SUBMIT(msgIn);
				} else {
					char *err_msg = "Missed messaging ack!";
					nf_app_error(ERR_MESSAGING, -ETIMEDOUT, err_msg,
						     strlen(err_msg));
				}
			} else if (received == 0) {
				socket_idle_count += SOCKET_POLL_INTERVAL;
				if (socket_idle_count > SOCK_RECV_TIMEOUT) {
					LOG_WRN("Socket receive timed out!");
					give_up_main_soc();
				}
			} else {
				char *e_msg = "Socket receive error!";
				nf_app_error(ERR_MESSAGING, -EIO, e_msg, strlen(e_msg));
				give_up_main_soc();
			}
		}
		k_sleep(K_SECONDS(SOCKET_POLL_INTERVAL));
	}
}

void listen_sock_poll(void)
{
	while (1) {
		if (k_sem_take(&listen_sem, K_FOREVER) == 0) {
			LOG_WRN("Waking up!");
			struct send_poll_request_now *wake_up = new_send_poll_request_now();
			EVENT_SUBMIT(wake_up);
		}
	}
}

static int start_tcp(void)
{
	int ret = modem_nf_wakeup();
	if (ret != 0) {
		LOG_ERR("Failed to wake up the modem!");
		return ret;
	}
	if (!fota_in_progress) {
		ret = check_ip();
		if (ret != 0) {
			LOG_ERR("Failed to get ip address!");
			char *e_msg = "Failed to get ip address!";
			nf_app_error(ERR_MESSAGING, -EIO, e_msg, strlen(e_msg));
			return ret;
		}
	}

	struct sockaddr_in addr4;
	if (IS_ENABLED(CONFIG_NET_IPV4)) {
		addr4.sin_family = AF_INET;
		addr4.sin_port = htons(server_port);
		inet_pton(AF_INET, server_ip, &addr4.sin_addr);

		ret = socket_connect(&conf.ipv4, (struct sockaddr *)&addr4, sizeof(addr4));
		if (ret < 0) {
			char *e_msg = "Socket connect error!";
			nf_app_error(ERR_MESSAGING, -ECONNREFUSED, e_msg, strlen(e_msg));
			return ret;
		}
	}
	return ret;
}

static uint8_t *CharMsgOut = NULL;
static bool cellular_controller_event_handler(const struct event_header *eh)
{
	if (is_messaging_ack_event(eh)) {
		k_free(pMsgIn);
		pMsgIn = NULL;
		k_sem_give(&messaging_ack);
		LOG_WRN("ACK received!");
		return false;
	} else if (is_messaging_stop_connection_event(eh)) {
		k_free(CharMsgOut);
		CharMsgOut = NULL;
		give_up_main_soc();
		struct cellular_ack_event *ack = new_cellular_ack_event();
		ack->message_sent = false;
		EVENT_SUBMIT(ack);
		return false;
#if defined(CONFIG_DIAGNOSTIC_EMS_FW)
	} else if (is_diag_thread_cntl_event(eh)) {
		struct diag_thread_cntl_event *event = cast_diag_thread_cntl_event(eh);
		diagnostic_run_cellular_thread = (event->run_cellular_thread == true);
		LOG_WRN("Cellular thread running = %d", diagnostic_run_cellular_thread);
		return false;
#endif
	} else if (is_messaging_host_address_event(eh)) {
		uint8_t port_length = 0;
		int ret = stg_config_str_read(STG_STR_HOST_PORT, server_address_tmp, &port_length);
		if (ret != 0) {
			LOG_ERR("Failed to read host address from ext flash");
		}

		struct messaging_host_address_event *event = cast_messaging_host_address_event(eh);
		memcpy(&server_address[0], event->address, sizeof(event->address) - 1);
		char *ptr_port;
		ptr_port = strchr(server_address, ':') + 1;
		server_port = atoi(ptr_port);
		if (server_port <= 0) {
			char *e_msg = "Failed to parse port number from new "
				      "host address!";
			nf_app_error(ERR_MESSAGING, -EILSEQ, e_msg, strlen(e_msg));
			return false;
		}
		uint8_t ip_len;
		ip_len = ptr_port - 1 - &server_address[0];
		memcpy(&server_ip[0], &server_address[0], ip_len);
		ret = memcmp(server_address, server_address_tmp, STG_CONFIG_HOST_PORT_BUF_LEN - 1);
		if (ret != 0) {
			LOG_INF("New host address received!");
			ret = stg_config_str_write(STG_STR_HOST_PORT, server_address,
						   STG_CONFIG_HOST_PORT_BUF_LEN - 1);
			if (ret != 0) {
				LOG_ERR("Failed to write new host address to ext flash!");
			}
		}
		return false;
	} else if (is_messaging_proto_out_event(eh)) {
		socket_idle_count = 0;
		struct messaging_proto_out_event *event = cast_messaging_proto_out_event(eh);
		uint8_t *pCharMsgOut = event->buf;
		size_t MsgOutLen = event->len;

		if (CharMsgOut == NULL) {
			/* make a local copy of the message to send.*/
			CharMsgOut = (char *)k_malloc(MsgOutLen);
			if (CharMsgOut == memcpy(CharMsgOut, pCharMsgOut, MsgOutLen)) {
				send_tcp_q(CharMsgOut, MsgOutLen);
				return false;
			}
		}
		struct cellular_ack_event *ack = new_cellular_ack_event();
		ack->message_sent = false;
		EVENT_SUBMIT(ack);
		LOG_WRN("Dropping message!");
		return false;
	} else if (is_messaging_mdm_fw_event(eh)) {
		struct messaging_mdm_fw_event *ev = cast_messaging_mdm_fw_event(eh);
		m_ftp_url = ev->buf;
		LOG_INF("%s", m_ftp_url);
		enable_mdm_fota = true;
		announce_connection_state(false);
		return false;
	} else if (is_check_connection(eh)) {
		if (enable_mdm_fota) {
			announce_connection_state(false);
			return false;
		}
		k_sem_give(&connection_state_sem);
		return false;
	} else if (is_free_message_mem_event(eh)) {
		k_free(CharMsgOut);
		CharMsgOut = NULL;
		struct cellular_ack_event *ack = new_cellular_ack_event();
		ack->message_sent = true;
		EVENT_SUBMIT(ack);
		waiting_for_msg = true;
		return false;
	} else if (is_dfu_status_event(eh)) {
		struct dfu_status_event *fw_upgrade_event = cast_dfu_status_event(eh);
		if (fw_upgrade_event->dfu_status == DFU_STATUS_IN_PROGRESS) {
			if (fota_in_progress)
				k_sem_give(&fota_progress_update);
			fota_in_progress = true;
			stop_rssi();
		} else {
			fota_in_progress = false;
			enable_rssi();
		}
		return false;
	} else if (is_pwr_status_event(eh)) {
		struct pwr_status_event *ev = cast_pwr_status_event(eh);
#if defined(CONFIG_DIAGNOSTIC_EMS_FW)
		if (ev->pwr_state != PWR_CHARGING) {
			battery_mv = ev->battery_mv;
		}
#endif
		if (ev->pwr_state < PWR_NORMAL && power_level_ok) {
			LOG_WRN("Will power off the modem!");
			power_level_ok = false;
		} else if (ev->pwr_state == PWR_NORMAL) {
			power_level_ok = true;
		}
		return false;
	} else if (is_save_modem_status_event(eh)) {
		struct save_modem_status_event *ev = cast_save_modem_status_event(eh);
		LOG_INF("Writing %d to modem status", ev->modem_status);
		if (stg_config_u8_write(STG_U8_MODEM_INSTALLING, ev->modem_status) != 0) {
			LOG_ERR("Writing to NVS fail");
		}
		return false;
	}
	return false;
}

/**
	 * Read full server address from storage and cache the ip as char array
	 * and the port as an unsigned int.
	 * @return: 0 on success, -1 on failure
	 */
int8_t cache_server_address(void)
{
	uint8_t port_length = 0;
	int err = stg_config_str_read(STG_STR_HOST_PORT, server_address, &port_length);
	if (err != 0 || server_address[0] == '\0') {
		return -1;
	}
	char *ptr_port;
	ptr_port = strchr(server_address, ':') + 1;
	server_port = atoi(ptr_port);
	if (server_port <= 0) {
		return -1;
	}
	uint8_t ip_len;
	ip_len = ptr_port - 1 - &server_address[0];
	memcpy(&server_ip[0], &server_address[0], ip_len);
	if (server_ip[0] != '\0') {
		LOG_INF("Host address read from storage: %s : %d", log_strdup(&server_ip[0]),
			server_port);
		return 0;
	} else {
		return -1;
	}
}

static int cellular_controller_connect(void *dev)
{
	int ret = lte_init();
	if (ret != 0) {
		LOG_ERR("Failed to start LTE connection. Check network interface! (%d)", ret);
		nf_app_error(ERR_CELLULAR_CONTROLLER, ret, NULL, 0);
		goto exit;
	}

	LOG_INF("Cellular network interface ready!");

	ret = cache_server_address();
	if (ret != 0) { //172.31.36.11:4321
		//172.31.33.243:9876
		strcpy(server_ip, "172.31.36.11");
		server_port = 4321;
		LOG_INF("Default server ip address will be "
			"used.");
	}

	ret = 0;

exit:
	return ret;
}

static void publish_gsm_info(void)
{
	struct gsm_info this_session_info;
	if (get_gsm_info(&this_session_info) == 0) {
		struct gsm_info_event *session_info = new_gsm_info_event();
		memcpy(&session_info->gsm_info, &this_session_info, sizeof(struct gsm_info));
		EVENT_SUBMIT(session_info);
	}
}

static void end_connection(void)
{
	int ret = stop_tcp(fota_in_progress, &connected);
	socket_idle_count = 0;
	if (ret != 0) {
		modem_is_ready = false;
	}
}

static mdm_fota_status mdm_fota(void)
{
	static bool download_complete = false;
	static bool mdm_install_started = false;
	int ret = INSTALLATION_FAILED;
	struct modem_nf_uftp_params ftp_params;
	ftp_params.ftp_server = "172.31.45.52";
	ftp_params.ftp_user = "sarar412";
	ftp_params.ftp_password = "Klave12005";
	ftp_params.download_timeout_sec = 3600;

	if (!download_complete) {
		LOG_INF("Starting FTP download, %s!", m_ftp_url);
		ret = modem_nf_ftp_fw_download(&ftp_params, m_ftp_url);
		if (ret == 0) {
			download_complete = true;
			LOG_INF("FTP download successful!");
			struct mdm_fw_update_event *ev = new_mdm_fw_update_event();
			ev->status = MDM_FW_DOWNLOAD_COMPLETE;
			EVENT_SUBMIT(ev);
			return MDM_FW_DOWNLOAD_COMPLETE;
		} else {
			LOG_WRN("FTP download failed!");
			struct mdm_fw_update_event *ev = new_mdm_fw_update_event();
			ev->status = DOWNLOAD_FAILED;
			EVENT_SUBMIT(ev);
			return DOWNLOAD_FAILED;
		}
	} else if (!mdm_install_started) {
		mdm_install_started = true;
		LOG_INF("Starting modem upgrade!");
		ret = modem_nf_ftp_fw_install(true);
		if (ret == 0) {
			LOG_INF("Modem upgrade successful!");
			return INSTALLATION_COMPLETE;
		} else {
			LOG_ERR("Modem upgrade failed!");
			return INSTALLATION_FAILED;
		}
	}
	return MDM_FOTA_STATUS_INIT;
}

static void cellular_controller_keep_alive(void *dev)
{
	int ret;
	static mdm_fota_status mdm_state = MDM_FOTA_STATUS_INIT;

#if defined(CONFIG_DIAGNOSTIC_EMS_FW)
	if (battery_mv >= 4000) {
		struct send_poll_request_now *wake_up = new_send_poll_request_now();
		EVENT_SUBMIT(wake_up);
	}
#endif

	while (true) {
#if defined(CONFIG_DIAGNOSTIC_EMS_FW)
		if (battery_mv >= 4000) {
			diagnostic_run_cellular_thread =
				true; //Force to run thread when battery voltage is above or equal to 4V
		}

		if (k_sem_take(&connection_state_sem, K_FOREVER) == 0 &&
		    diagnostic_run_cellular_thread) {
#else
		if (k_sem_take(&connection_state_sem, K_FOREVER) == 0) {
#endif
			if (enable_mdm_fota) {
				mdm_state = mdm_fota();

				switch (mdm_state) {
				case MDM_FW_DOWNLOAD_COMPLETE:
					enable_mdm_fota = false;
					end_connection();
					goto update_connection_state;
					break;
				case INSTALLATION_COMPLETE:
					modem_is_ready = false;
					break;
				case DOWNLOAD_FAILED:
					enable_mdm_fota = false;
					end_connection();
					break;
				default:
					enable_mdm_fota = false;
					LOG_INF("Modem FW upgrade was NOT successful!");
				}
			}

			if (!power_level_ok) {
				connected = false;
				modem_is_ready = false;
				ret = modem_nf_pwr_off();
				if (ret != 0) {
					if (ret == -EALREADY) {
						LOG_WRN("Modem suspended!");
					} else {
						LOG_ERR("Failed to switch off"
							" modem!");
					}
				} else {
					LOG_WRN("Modem switched off!");
				}
				goto update_connection_state;
			}
			socket_idle_count = 0;
			if (!cellular_controller_is_ready()) {
				/* reset flags to avoid hanging the
				 * communication with the
				 * server in case the http download client
				 * fails ungracefully.*/
				connected = false;
				fota_in_progress = false;
				ret = reset_modem();
				if (ret == 0) {
					if (mdm_state == INSTALLATION_COMPLETE) {
						struct mdm_fw_update_event *ev =
							new_mdm_fw_update_event();
						ev->status = INSTALLATION_COMPLETE;
						EVENT_SUBMIT(ev);
						mdm_state = MDM_FOTA_STATUS_INIT;
					}
					publish_gsm_info();
					ret = cellular_controller_connect(dev);
					if (ret == 0) {
						modem_is_ready = true;
					}
				}
				enable_mdm_fota = false;
			}
			if (cellular_controller_is_ready()) {
				if (!connected) { //check_ip
					// takes place in start_tcp() in this
					// case.
					if (fota_in_progress) {
						k_sem_reset(&fota_progress_update);
						if (k_sem_take(&fota_progress_update,
							       K_SECONDS(10)) != 0) {
							LOG_WRN("FOTA prgress"
								" missed!");
						}
					}
					ret = start_tcp();
					if (ret >= 0) {
						connected = true;
						socket_idle_count = 0;
						k_sem_reset(&close_main_socket_sem);
					} else {
						modem_is_ready = false;
						fota_in_progress = false;
						end_connection(); /* ungraceful */
					}
				} else {
					socket_idle_count = 0;
					if (k_sem_take(&close_main_socket_sem, K_NO_WAIT) != 0) {
						if (!fota_in_progress) { /* skip ip check when FOTA is in
 * progress- */
							ret = check_ip();
							if (ret != 0) {
								end_connection(); /* ungraceful */
							}
						}
					} else {
						end_connection(); /* graceful */
					}
				}
			} else {
				modem_nf_pwr_off();
				connected = false;
				fota_in_progress = false;
			}
		update_connection_state:
			announce_connection_state(connected);
		}
#if defined(CONFIG_DIAGNOSTIC_EMS_FW)
		else {
			announce_connection_state(false);
		}
#endif
	}
}

void announce_connection_state(bool state)
{
	k_sem_reset(&connection_state_sem);
	struct connection_state_event *ev = new_connection_state_event();
	ev->state = state;
	EVENT_SUBMIT(ev);
	socket_idle_count = 0;
	if (state == true) {
		struct modem_state *modem_active = new_modem_state();
		modem_active->mode = POWER_ON;
		EVENT_SUBMIT(modem_active);
	}
	publish_gsm_info();
}

bool cellular_controller_is_ready(void)
{
	return modem_is_ready;
}
static uint8_t g_modem_status_cache = 0x00;

static int modem_read_status_nvm(uint8_t *status)
{
	if (g_modem_status_cache != 0) {
		*status = g_modem_status_cache;
		return 0;
	}
	return -1;
}

static int modem_write_status_nvm(uint8_t status)
{
	g_modem_status_cache = status;
	/* Post event */
	struct save_modem_status_event *ev = new_save_modem_status_event();
	ev->modem_status = status;
	EVENT_SUBMIT(ev);
	return 0;
}

int8_t cellular_controller_init(void)
{
	connected = false;
	const struct device *gsm_dev = bind_modem();
	if (gsm_dev == NULL) {
		LOG_ERR("GSM driver was not found! (%d)", -ENODEV);
		nf_app_error(ERR_CELLULAR_CONTROLLER, -ENODEV, NULL, 0);
		return -1;
	}
	if (stg_config_u8_read(STG_U8_MODEM_INSTALLING, &g_modem_status_cache) != 0) {
		LOG_ERR("Error retrieving STG_U8_MODEM_INSTALLING from NVS");
	}
	LOG_INF("modem status read: %d", g_modem_status_cache);
	set_modem_status_cb(modem_read_status_nvm, modem_write_status_nvm);

	/* Start connection keep-alive thread */
	modem_is_ready = false;
	k_sem_init(&connection_state_sem, 0, 1);
	k_thread_create(&keep_alive_thread, keep_alive_stack,
			K_KERNEL_STACK_SIZEOF(keep_alive_stack),
			(k_thread_entry_t)cellular_controller_keep_alive, (void *)gsm_dev, NULL,
			NULL, K_PRIO_COOP(CONFIG_CELLULAR_KEEP_ALIVE_THREAD_PRIORITY), 0,
			K_NO_WAIT);

	return 0;
}

/** put a message in the send out queue
 * The queue can hold a single message and it will be discarded on arrival of
 * a new meassage.
 * .*/
static int send_tcp_q(char *msg, size_t len)
{
	LOG_DBG("send_tcp_q start!");
	struct msg2server msgout;
	msgout.msg = msg;
	msgout.len = len;
	LOG_DBG("send_tcp_q allocated msgout!");
	while (k_msgq_put(&msgq, &msgout, K_NO_WAIT) != 0) {
		/* Message queue is full: purge old data & try again */
		k_msgq_purge(&msgq);
	}
	LOG_DBG("message successfully pushed to queue!");
	return 0;
}

static void send_tcp_fn(void)
{
	while (true) {
		int rc = k_poll(events, 1, K_FOREVER);
		if (rc == 0) {
			while (k_msgq_num_used_get(&msgq) > 0) {
				struct msg2server msg_in_q;
				k_msgq_get(&msgq, &msg_in_q, K_FOREVER);
				int ret = send_tcp(msg_in_q.msg, msg_in_q.len);
				if (ret != msg_in_q.len) {
					LOG_WRN("Failed to send TCP message!");
					struct messaging_stop_connection_event *end_connection =
						new_messaging_stop_connection_event();
					EVENT_SUBMIT(end_connection);
				} else {
					struct free_message_mem_event *ev =
						new_free_message_mem_event();
					EVENT_SUBMIT(ev);
				}
			}
			events[0].state = K_POLL_STATE_NOT_READY;
		}
	}
}

EVENT_LISTENER(MODULE, cellular_controller_event_handler);
EVENT_SUBSCRIBE(MODULE, messaging_ack_event);
EVENT_SUBSCRIBE(MODULE, messaging_proto_out_event);
EVENT_SUBSCRIBE(MODULE, messaging_mdm_fw_event);
EVENT_SUBSCRIBE(MODULE, messaging_stop_connection_event);
EVENT_SUBSCRIBE(MODULE, messaging_host_address_event);
EVENT_SUBSCRIBE(MODULE, check_connection);
EVENT_SUBSCRIBE(MODULE, free_message_mem_event);
EVENT_SUBSCRIBE(MODULE, dfu_status_event);
EVENT_SUBSCRIBE(MODULE, pwr_status_event);
#if defined(CONFIG_DIAGNOSTIC_EMS_FW)
EVENT_SUBSCRIBE(MODULE, diag_thread_cntl_event);
#endif
