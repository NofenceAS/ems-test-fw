
#include <devicetree.h>
#include <event_manager.h>
#include <logging/log.h>
#include <sys/printk.h>
#include <zephyr.h>
#include <nrf.h>

#include "error_event.h"
#include "diagnostics.h"
#include "selftest.h"
#include "fw_upgrade.h"
#include "fw_upgrade_events.h"
#include "stg_config.h"
#include "ble_controller.h"
#include "cellular_controller.h"
#include "ep_module.h"
#include "amc_handler.h"
#include "nf_settings.h"
#include "buzzer.h"
#include "pwr_module.h"
#include "pwr_event.h"

#if defined(CONFIG_WATCHDOG_ENABLE)
#include "watchdog_app.h"
#endif

#include "messaging.h"

#if CONFIG_GNSS_CONTROLLER
#include "gnss_controller.h"
#endif

#include "storage.h"
#include "nf_version.h"
#include "env_sensor_event.h"
#include "module_state_event.h"
#include "movement_controller.h"
#include "movement_events.h"
#include "time_use.h"

/* Fetched from 52840 datasheet RESETREAS register. */
#define SOFT_RESET_REASON_FLAG (1 << 2)
#define MODULE main
LOG_MODULE_REGISTER(MODULE, CONFIG_LOG_DEFAULT_LEVEL);

/**
 * The Nofence X3 main entry point. This is
 * called from the Zephyr kernel.
 */
void main(void)
{
	/* Fetch reset reason, log in the future? */
	uint32_t reset_reason = NRF_POWER->RESETREAS;
	NRF_POWER->RESETREAS = NRF_POWER->RESETREAS;

	int err;
	LOG_INF("Starting Nofence application...");

	selftest_init();

	/* Initialize diagnostics module. */
#if CONFIG_DIAGNOSTICS
	err = diagnostics_module_init();
	if (err) {
		char *e_msg = "Could not initialize diagnostics module";
		LOG_ERR("%s (%d)", log_strdup(e_msg), err);
		nf_app_error(ERR_DIAGNOSTIC, err, e_msg, strlen(e_msg));
	}
#endif

	err = stg_config_init();
	if (err != 0) {
		LOG_ERR("STG Config failed to initialize");
	}

/* Not all boards have eeprom */
#if DT_NODE_HAS_STATUS(DT_ALIAS(eeprom), okay)
	const struct device *eeprom_dev = DEVICE_DT_GET(DT_ALIAS(eeprom));
	selftest_mark_state(SELFTEST_EEPROM_POS, eeprom_dev != NULL);
	if (eeprom_dev == NULL) {
		char *e_msg = "No EEPROM device detected!";
		LOG_ERR("%s (%d)", log_strdup(e_msg), -EIO);
		nf_app_error(ERR_EEPROM, -EIO, e_msg, strlen(e_msg));
	}
	eep_init(eeprom_dev);
	/* Fetch and log stored serial number */
	uint32_t serial_id = 0;
	err = stg_config_u32_read(STG_U32_UID, &serial_id);
	if (serial_id) {
		LOG_INF("Device Serial Number stored in ext flash: %d", serial_id);
	} else {
		LOG_WRN("Missing device Serial Number in ext flash");
	}
#endif

	/* Initialize the power manager module. */
	err = pwr_module_init();
	if (err) {
		char *e_msg = "Could not initialize the power module";
		LOG_ERR("%s (%d)", log_strdup(e_msg), err);
		nf_app_error(ERR_PWR_MODULE, err, e_msg, strlen(e_msg));
	}

	/* Initialize the buzzer */
	err = buzzer_module_init();
	if (err) {
		char *e_msg = "Could not initialize the buzzer module";
		LOG_ERR("%s (%d)", log_strdup(e_msg), err);
		nf_app_error(ERR_SOUND_CONTROLLER, err, e_msg, strlen(e_msg));
	}

	/* Initialize the event manager. */
	err = event_manager_init();
	if (err) {
		LOG_ERR("Could not initialize event manager (%d)", err);
		return;
	}

	bool is_soft_reset = ((reset_reason & SOFT_RESET_REASON_FLAG) != 0);
	uint8_t soft_reset_reason;
	if (pwr_module_reboot_reason(&soft_reset_reason) != 0) {
		soft_reset_reason = REBOOT_UNKNOWN;
	}
	int bat_percent = fetch_battery_percent();

	LOG_ERR("Was soft reset ?:%i, Soft reset reason:%d, Battery percent:%i",
		is_soft_reset, soft_reset_reason, bat_percent);

	/* If not set, we can play the sound. */
	if ((is_soft_reset != true) ||
	    ((is_soft_reset == true) &&
	     (soft_reset_reason == REBOOT_BLE_RESET))) {
		if (bat_percent > 20) {
			if (bat_percent >= 75) {
				/* Play battery sound. */
				struct sound_event *sound_ev =
					new_sound_event();
				sound_ev->type = SND_SHORT_100;
				EVENT_SUBMIT(sound_ev);
			}

			/* Wait for battery percent to finish, since sound controller 
			 * doesn't queue sounds. */
			k_sleep(K_MSEC(200));

			/* Play welcome sound. */
			/*TODO: only play when battery level is adequate.*/
			struct sound_event *sound_ev = new_sound_event();
			sound_ev->type = SND_WELCOME;
			EVENT_SUBMIT(sound_ev);
		}
	}

	/* Initialize the watchdog */
#if defined(CONFIG_WATCHDOG_ENABLE)
	err = watchdog_init_and_start();
	if (err) {
		char *e_msg = "Could not initialize and start watchdog";
		LOG_ERR("%s (%d)", log_strdup(e_msg), err);
		nf_app_error(ERR_WATCHDOG, err, e_msg, strlen(e_msg));
	}
#endif

	err = stg_init_storage_controller();
	selftest_mark_state(SELFTEST_FLASH_POS, err == 0);
	if (err) {
		LOG_ERR("Could not initialize storage controller (%d)", err);
		return;
	}

	/* Initialize BLE module. */
	err = ble_module_init();
	if (err) {
		char *e_msg = "Could not initialize BLE module.";
		LOG_ERR("%s (%d)", log_strdup(e_msg), err);
		nf_app_error(ERR_BLE_MODULE, err, e_msg, strlen(e_msg));
	}

	/* Initialize firmware upgrade module. */
	err = fw_upgrade_module_init();
	if (err) {
		char *e_msg = "Could not initialize firmware upgrade module";
		LOG_ERR("%s (%d)", log_strdup(e_msg), err);
		nf_app_error(ERR_FW_UPGRADE, err, e_msg, strlen(e_msg));
	}

	/* Initialize the electric pulse module */
	err = ep_module_init();
	if (err) {
		char *e_msg = "Could not initialize electric pulse module";
		LOG_ERR("%s (%d)", log_strdup(e_msg), err);
		nf_app_error(ERR_EP_MODULE, err, e_msg, strlen(e_msg));
	}

	/* Important to initialize external storage first, since we use the sleep 
	 * sigma value from storage when we init.
	 */
	err = init_movement_controller();
	selftest_mark_state(SELFTEST_ACCELEROMETER_POS, err == 0);
	if (err) {
		char *e_msg = "Could not initialize the movement module";
		LOG_ERR("%s (%d)", log_strdup(e_msg), err);
		nf_app_error(ERR_MOVEMENT_CONTROLLER, err, e_msg,
			     strlen(e_msg));
	}

	/* Initialize the cellular controller */
	err = cellular_controller_init();
	if (err) {
		char *e_msg = "Could not initialize the cellular controller";
		LOG_ERR("%s (%d)", log_strdup(e_msg), err);
		nf_app_error(ERR_CELLULAR_CONTROLLER, err, e_msg,
			     strlen(e_msg));
	}
	/* Initialize the time module used for the histogram calculation */
	err = time_use_module_init();
	if (err) {
		LOG_ERR("Could not initialize time use module. %d", err);
	}

	/* Initialize the messaging module */
	err = messaging_module_init();
	if (err) {
		char *e_msg = "Could not initialize the messaging module.";
		LOG_ERR("%s (%d)", log_strdup(e_msg), err);
		nf_app_error(ERR_MESSAGING, err, e_msg, strlen(e_msg));
	}

	/* Initialize the GNSS controller */
#if CONFIG_GNSS_CONTROLLER
	err = gnss_controller_init();
	selftest_mark_state(SELFTEST_GNSS_POS, err == 0);
	if (err) {
		char *e_msg = "Could not initialize the GNSS controller.";
		LOG_ERR("%s (%d)", log_strdup(e_msg), err);
		nf_app_error(ERR_GNSS_CONTROLLER, err, e_msg, strlen(e_msg));
	}
#endif

	/* Initialize animal monitor control module, depends on storage
	 * controller to be initialized first since amc sends
	 * a request for pasture data on init.
	 */
	err = amc_module_init();
	if (err) {
		char *e_msg = "Could not initialize the AMC module";
		LOG_ERR("%s (%d)", log_strdup(e_msg), err);
		nf_app_error(ERR_AMC, err, e_msg, strlen(e_msg));
	}

	/* Once EVERYTHING is initialized correctly and we get connection to
	 * server, we can mark the image as valid. If we do not mark it as valid,
	 * it will revert to the previous version on the next reboot that occurs.
	 */
	mark_new_application_as_valid();
	LOG_INF("----- Build time: " __DATE__ " " __TIME__ " -----");
	LOG_INF("Booted application firmware version %i, and marked it as valid. Reset reason %i",
		NF_X25_VERSION_NUMBER, reset_reason);
}
