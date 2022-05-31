
#include <devicetree.h>
#include <event_manager.h>
#include <logging/log.h>
#include <sys/printk.h>
#include <zephyr.h>

#include "error_event.h"

#include "diagnostics.h"
#include "fw_upgrade.h"
#include "fw_upgrade_events.h"
#include "nf_settings.h"
#include "ble_controller.h"
#include "cellular_controller.h"
#include "ep_module.h"
#include "amc_handler.h"
#include "nf_settings.h"
#include "buzzer.h"
#include "pwr_module.h"
#if defined(CONFIG_WATCHDOG_ENABLE)
#include "watchdog_app.h"
#endif

#include "messaging.h"
#include "cellular_controller.h"
#include "error_event.h"

#if CONFIG_GNSS_CONTROLLER
#include "gnss_controller.h"
#endif

#include "storage.h"
#include "nf_version.h"

#include "env_sensor_event.h"

#define MODULE main
#include "module_state_event.h"

#include "movement_controller.h"
#include "movement_events.h"
#include "time_use.h"

LOG_MODULE_REGISTER(MODULE, CONFIG_LOG_DEFAULT_LEVEL);

/**
 * The Nofence X3 main entry point. This is
 * called from the Zephyr kernel.
 */
void main(void)
{
	int err;
	LOG_INF("Starting Nofence application...");

	err = stg_init_storage_controller();
	if (err) {
		LOG_ERR("Could not initialize storage controller (%d)", err);
		return;
	}

	/* Initialize the event manager. */
	err = event_manager_init();
	if (err) {
		LOG_ERR("Could not initialize event manager (%d)", err);
		return;
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

/* Not all boards have eeprom */
#if DT_NODE_HAS_STATUS(DT_ALIAS(eeprom), okay)
	const struct device *eeprom_dev = DEVICE_DT_GET(DT_ALIAS(eeprom));
	if (eeprom_dev == NULL) {
		char *e_msg = "No EEPROM device detected!";
		LOG_ERR("%s (%d)", log_strdup(e_msg), -EIO);
		nf_app_error(ERR_EEPROM, -EIO, e_msg, strlen(e_msg));
	}
	eep_init(eeprom_dev);
	/* Fetch and log stored serial number */
	uint32_t serial_id = 0;
	err = eep_uint32_read(EEP_UID, &serial_id);
	if (serial_id) {
		LOG_INF("Device Serial Number stored in EEPROM: %d", serial_id);
	} else {
		LOG_WRN("Missing device Serial Number in EEPROM");
	}

#endif

	/* Initialize diagnostics module. */
#if CONFIG_DIAGNOSTICS
	err = diagnostics_module_init();
	if (err) {
		char *e_msg = "Could not initialize diagnostics module";
		LOG_ERR("%s (%d)", log_strdup(e_msg), err);
		nf_app_error(ERR_DIAGNOSTIC, err, e_msg, strlen(e_msg));
	}
#endif

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

	/* Important to initialize the eeprom first, since we use the 
	 * sleep sigma value from eeprom when we init.
	 */
	err = init_movement_controller();
	if (err) {
		char *e_msg = "Could not initialize the movement module";
		LOG_ERR("%s (%d)", log_strdup(e_msg), err);
		nf_app_error(ERR_MOVEMENT_CONTROLLER, err, e_msg,
			     strlen(e_msg));
	}

	/* Play welcome sound. */
	/*TODO: only play when battery level is adequate.*/
	struct sound_event *sound_ev = new_sound_event();
	sound_ev->type = SND_WELCOME;
	EVENT_SUBMIT(sound_ev);

	/* Initialize the cellular controller */
	err = cellular_controller_init();
	if (err) {
		char *e_msg = "Could not initialize the cellular controller";
		LOG_ERR("%s (%d)", log_strdup(e_msg), err);
		nf_app_error(ERR_CELLULAR_CONTROLLER, err, e_msg,
			     strlen(e_msg));
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
	if (err) {
		char *e_msg = "Could not initialize the GNSS controller.";
		LOG_ERR("%s (%d)", log_strdup(e_msg), err);
		nf_app_error(ERR_GNSS_CONTROLLER, err, e_msg, strlen(e_msg));
	}
#endif
	/* Initialize the time module used for the histogram calculation */
	err = time_use_module_init();
	if (err) {
		LOG_ERR("Could not initialize time use module. %d", err);
	}

	/* Once EVERYTHING is initialized correctly and we get connection to
	 * server, we can mark the image as valid. If we do not mark it as valid,
	 * it will revert to the previous version on the next reboot that occurs.
	 */
	mark_new_application_as_valid();

	LOG_INF("Booted application firmware version %i, and marked it as valid.",
		NF_X25_VERSION_NUMBER);
}
