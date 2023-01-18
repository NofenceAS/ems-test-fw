#include "cmd_system.h"
#include "selftest.h"
#include "log_backend_diag.h"
#include "diagnostics_events.h"
#include <stddef.h>
#include <string.h>
#include "gnss.h"
#include "gnss_controller_events.h"
#include "modem_nf.h"
#include "pwr_event.h"
#include "cellular_helpers_header.h"
#include "messaging_module_events.h"
#include "storage.h"
#include "diagnostic_flags.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(diag_cmd_system, 4);

int commander_system_handler(enum diagnostics_interface interface, uint8_t cmd, uint8_t *data,
			     uint32_t size)
{
	int err = 0;

	uint8_t resp = ACK;

	switch (cmd) {
	case PING: {
		commander_send_resp(interface, SYSTEM, cmd, DATA, data, size);
		break;
	}
	case LOG: {
		if (size < 1) {
			resp = NOT_ENOUGH;
			commander_send_resp(interface, SYSTEM, cmd, resp, NULL, 0);
			err = -EINVAL;
		} else {
			if (data[0]) {
				log_backend_diag_enable(interface);
			} else {
				log_backend_diag_disable();
			}
			resp = ACK;
			commander_send_resp(interface, SYSTEM, cmd, resp, NULL, 0);
		}
		break;
	}
	case TEST: {
		uint32_t test_buf[2];
		selftest_get_result(&test_buf[0], &test_buf[1]);

		commander_send_resp(interface, SYSTEM, cmd, DATA, (uint8_t *)test_buf,
				    sizeof(test_buf));
		break;
	}
	case SLEEP: {
		resp = ACK;
		int ret = 0;

		LOG_DBG("Setting device in sleep...");

		const struct device *gnss_dev = NULL;
		gnss_dev = DEVICE_DT_GET(DT_ALIAS(gnss));
		ret = gnss_set_backup_mode(gnss_dev);
		if (ret < 0) {
			LOG_ERR("Could not set GNSS in backup: %d", ret);
			resp = ERROR;
		}
		//struct gnss_set_mode_event *ev = new_gnss_set_mode_event();
		//ev->mode = GNSSMODE_INACTIVE;
		//EVENT_SUBMIT(ev);

		ret = stop_tcp(false);
		if (ret < 0) {
			LOG_ERR("Could not set modem in sleep: %d", ret);
			resp = ERROR;
		}

		LOG_DBG("Device is sleeping");

		commander_send_resp(interface, SYSTEM, cmd, resp, NULL, 0);
		break;
	}
	case WAKEUP: {
		resp = ACK;
		int ret = 0;

		LOG_DBG("Waking up device...");

		const struct device *gnss_dev = NULL;
		gnss_dev = DEVICE_DT_GET(DT_ALIAS(gnss));
		ret = gnss_wakeup(gnss_dev);
		if (ret < 0) {
			LOG_ERR("Could wake up GNSS: %d", ret);
			resp = ERROR;
		}
		//struct gnss_set_mode_event *ev = new_gnss_set_mode_event();
		//ev->mode = GNSSMODE_MAX;
		//EVENT_SUBMIT(ev);

		ret = reset_modem();
		if (ret < 0) {
			LOG_ERR("Could not wake up modem: %d", ret);
			resp = ERROR;
		}
		LOG_DBG("Device woke up");

		commander_send_resp(interface, SYSTEM, cmd, resp, NULL, 0);
		break;
	}
	case REBOOT: {
		resp = ACK;

		struct pwr_reboot_event *r_ev = new_pwr_reboot_event();
		r_ev->reason = REBOOT_NO_REASON;
		EVENT_SUBMIT(r_ev);

		commander_send_resp(interface, SYSTEM, cmd, resp, NULL, 0);
		break;
	}
	case THREAD_CONTROL: {
		resp = ACK;

		struct diag_thread_cntl_event *diag = new_diag_thread_cntl_event();
		uint8_t tc = data[0];
		LOG_WRN("THREAD CONTROL = %d", tc);
		if (tc & (1 << 0)) {
			diag->run_cellular_thread = true; //Cellular thread ON
			LOG_WRN("CEL ON");
		}
		if (tc & (0 << 0)) {
			diag->run_cellular_thread = false; //Cellular thread OFF
			LOG_WRN("CEL OFF");
		}
		if (tc & (1 << 1)) {
			diag->allow_fota = true; //Cellular thread OFF
			LOG_WRN("FOTA ON");
		}
		if (tc & (0 << 1)) {
			diag->allow_fota = false; //Cellular thread OFF
			LOG_WRN("FOTA OFF");
		}
		EVENT_SUBMIT(diag);

		commander_send_resp(interface, SYSTEM, cmd, resp, NULL, 0);
		break;
	}
	case FORCE_POLL_REQ: {
		resp = ACK;
		LOG_DBG("Forcing poll request");

		struct send_poll_request_now *wake_up = new_send_poll_request_now();
		EVENT_SUBMIT(wake_up);

		commander_send_resp(interface, SYSTEM, cmd, resp, NULL, 0);
		break;
	}
	case CLEAR_PASTURE: {
		LOG_INF("clearing pasture partition...");
		err = stg_clear_partition(STG_PARTITION_PASTURE);
		if (err) {
			LOG_ERR("could not clear pasture partition: %d", err);
			resp = ERROR;
		} else {
			LOG_INF("pasture partition cleared!");
		}
		commander_send_resp(interface, SYSTEM, cmd, resp, NULL, 0);
		break;
	}
	case ERASE_FLASH: {
		LOG_INF("erasing flash...");
		resp = ACK;

		err = stg_clear_partition(STG_PARTITION_LOG);
		if (err) {
			LOG_ERR("could not clear LOG partition: %d", err);
			resp = ERROR;
		}
		err = stg_clear_partition(STG_PARTITION_ANO);
		if (err) {
			LOG_ERR("could not clear ANO partition: %d", err);
			resp = ERROR;
		}
		err = stg_clear_partition(STG_PARTITION_PASTURE);
		if (err) {
			LOG_ERR("could not clear PASTURE partition: %d", err);
			resp = ERROR;
		}
		err = stg_clear_partition(STG_PARTITION_SYSTEM_DIAG);
		if (err) {
			LOG_ERR("could not clear SYSTEM_DIAG partition: %d", err);
			resp = ERROR;
		}

		struct update_flash_erase *ev = new_update_flash_erase();
		EVENT_SUBMIT(ev);

		if (resp == ACK) {
			LOG_INF("flash erased!");
		} else {
			LOG_ERR("could not erase flash");
		}

		commander_send_resp(interface, SYSTEM, cmd, resp, NULL, 0);
		break;
	}
	case GET_DIAG_FLAGS:
	{
	
	}
	case SET_DIAG_FLAGS:
	{
		
	}
	case CLR_DIAG_FLAGS:
	{
		
	}
	default:
		resp = UNKNOWN_CMD;
		commander_send_resp(interface, SYSTEM, cmd, resp, NULL, 0);
		break;
	}

	return err;
}
