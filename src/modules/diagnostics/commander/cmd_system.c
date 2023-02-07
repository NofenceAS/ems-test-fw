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
#include "approtect.h"

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

		LOG_DBG("Setting device in sleep by setting GNSS to inactive...");

		struct diag_thread_cntl_event *diag = new_diag_thread_cntl_event();
		diag->allow_fota = false;
		diag->run_cellular_thread = false;
		diag->force_gnss_mode = 1; //GNSS INACTIVE
		EVENT_SUBMIT(diag);

		struct gnss_set_mode_event *ev = new_gnss_set_mode_event();
		ev->mode = diag->force_gnss_mode;
		EVENT_SUBMIT(ev);

		commander_send_resp(interface, SYSTEM, cmd, resp, NULL, 0);
		break;
	}
	case WAKEUP: {
		resp = ACK;

		LOG_DBG("Waking up device by setting GNSS to max...");

		struct diag_thread_cntl_event *diag = new_diag_thread_cntl_event();
		diag->force_gnss_mode = 4; //GNSS Max
		EVENT_SUBMIT(diag);

		struct gnss_set_mode_event *ev = new_gnss_set_mode_event();
		ev->mode = diag->force_gnss_mode;
		EVENT_SUBMIT(ev);

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
		} else {
			diag->run_cellular_thread = false; //Cellular thread OFF
			LOG_WRN("CEL OFF");
		}

		if (tc & (1 << 1)) {
			diag->allow_fota = true; //Cellular thread OFF
			LOG_WRN("FOTA ON");
		} else {
			diag->allow_fota = false; //Cellular thread OFF
			LOG_WRN("FOTA OFF");
		}

		if ((tc >> 2) == 1) {
			diag->force_gnss_mode = 1; //GNSS INACTIVE
			LOG_WRN("GNSS INACTIVE");
		} else if ((tc >> 2) == 2) {
			diag->force_gnss_mode = 2; //GNSS PSM
			LOG_WRN("GNSS PSM");
		} else if ((tc >> 2) == 4) {
			diag->force_gnss_mode = 4; //GNSS Max
			LOG_WRN("GNSS MAX");
		}

		EVENT_SUBMIT(diag);

		struct gnss_set_mode_event *ev = new_gnss_set_mode_event();
		ev->mode = diag->force_gnss_mode;
		EVENT_SUBMIT(ev);

		commander_send_resp(interface, SYSTEM, cmd, resp, NULL, 0);
		break;
	}
	case READ_THREAD_CONTROL: {
		resp = NOT_IMPLEMENTED;
		uint8_t tc = 0;

		/* not implemented, could add subscription in diagnostics
		   and add status to onboard_data */

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

		err = stg_config_u8_write(STG_U8_TEACH_MODE_FINISHED, 0);
		err = stg_config_u8_write(STG_U8_KEEP_MODE, 0);
		err = stg_config_u16_write(STG_U16_ZAP_CNT_TOT, 0);
		err = stg_config_u16_write(STG_U16_ZAP_CNT_DAY, 0);
		err = stg_config_u32_write(STG_U32_WARN_CNT_TOT, 0);

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
	case SET_DIAG_FLAGS: {
	}
	case GET_DIAG_FLAGS: {
	}
	case CLR_DIAG_FLAGS: {
		resp = DATA;

		if (cmd == SET_DIAG_FLAGS) {
			if (size >= 4) {
				uint32_t flags_in = (data[0] << 0) + (data[1] << 8) +
						    (data[2] << 16) + (data[3] << 24);
				err = diagnostic_set_flag(flags_in);
				if (err != 0) {
					resp = ERROR;
					LOG_ERR("ERROR SETTING FLAGS: %u", flags_in);
				} else {
					LOG_WRN("FLAGS SET: %u", flags_in);
				}
			} else {
				resp = NOT_ENOUGH;
			}
		} else if (cmd == CLR_DIAG_FLAGS) {
			if (size >= 4) {
				uint32_t flags_in = (data[0] << 0) + (data[1] << 8) +
						    (data[2] << 16) + (data[3] << 24);
				err = diagnostic_clear_flag(flags_in);
				if (err != 0) {
					resp = ERROR;
					LOG_ERR("ERROR CLEARING FLAGS: %u", flags_in);
				} else {
					LOG_WRN("FLAGS CLEARED: %u", flags_in);
				}
			} else {
				err = diagnostic_clear_flags();
				if (err != 0) {
					resp = ERROR;
					LOG_ERR("ERROR CLEARING ALL FLAGS");
				} else {
					LOG_WRN("ALL FLAGS CLEARED");
				}
			}
		}

		uint8_t buffer[4] = { 0 };
		uint32_t flags_out = 0;
		err = diagnostic_get_flags(&flags_out);
		if (err != 0) {
			resp = ERROR;
			LOG_ERR("ERROR READING FLAGS: %u", flags_out);
		} else {
			memcpy(&buffer, &flags_out, sizeof(uint32_t));
			LOG_WRN("FLAGS READ: %u", flags_out);
		}

		commander_send_resp(interface, SYSTEM, cmd, resp, buffer, sizeof(uint32_t));
		break;
	}
	case SET_LOCK_BIT: {
		resp = ACK;
		LOG_WRN("Setting firmware protection lock bit!");
		lock_approtect();
		LOG_WRN("Protection lock bit set!");
		commander_send_resp(interface, SYSTEM, cmd, resp, NULL, 0);
		break;
	}
	default:
		resp = UNKNOWN_CMD;
		commander_send_resp(interface, SYSTEM, cmd, resp, NULL, 0);
		break;
	}

	return err;
}
