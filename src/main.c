
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
		((is_soft_reset == true) && (soft_reset_reason == REBOOT_BLE_RESET))) {
		if (bat_percent > 10) {
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

	/* Once EVERYTHING is initialized correctly and we get connection to
	 * server, we can mark the image as valid. If we do not mark it as valid,
	 * it will revert to the previous version on the next reboot that occurs.
	 */
	mark_new_application_as_valid();

	LOG_INF("Booted application firmware version %i, and marked it as valid. Reset reason %i",
		NF_X25_VERSION_NUMBER, reset_reason);


	/* EEP vs STG u8 */
	int ret;
	uint8_t eep_u8_val;
	uint8_t stg_u8_val;
	for (int i = 0; i < (EEP_RESET_REASON + 1); i++)
	{
		/* Read EEP */
		eep_u8_val = 0;
		ret = eep_uint8_read(i, &eep_u8_val);
		if (ret != 0) 
		{
			LOG_ERR("EEP(u8) Failed to read id %d, err %d", i, ret);
		}
		/* Read STG */
		stg_u8_val = 0;
		ret = stg_config_u8_read(i, &stg_u8_val);
		if (ret != 0) 
		{
			LOG_ERR("STG(u8) Failed to read id %d, err %d", i, ret);
		}
		LOG_INF("EEPvsSTG(u8): Id = %d, EEP = %d, STG = %d", i, eep_u8_val, stg_u8_val);
	}
	uint16_t eep_u16_val;
	uint16_t stg_u16_val;
	for (int i = 0; i < (EEP_PRODUCT_TYPE + 1); i++)
	{
		/* Read EEP */
		eep_u16_val = 0;
		ret = eep_uint16_read(i, &eep_u16_val);
		if (ret != 0) 
		{
			LOG_ERR("EEP(u16) Failed to read id %d, err %d", i, ret);
		}
		/* Read STG */
		stg_u16_val = 0;
		ret = stg_config_u16_read(i+14, &stg_u16_val);
		if (ret != 0) 
		{
			LOG_ERR("STG(u16) Failed to read id %d(+14), err %d", i, ret);
		}
		LOG_INF("EEPvsSTG(u16): Id = %d(+14), EEP = %d, STG = %d", i, eep_u16_val, stg_u16_val);
	}
	uint32_t eep_u32_val;
	uint32_t stg_u32_val;
	for (int i = 0; i < (EEP_WARN_CNT_TOT + 1); i++)
	{
		/* Read EEP */
		eep_u32_val = 0;
		ret = eep_uint32_read(i, &eep_u32_val);
		if (ret != 0) 
		{
			LOG_ERR("EEP(u32) Failed to read id %d, err %d", i, ret);
		}
		/* Read STG */
		stg_u32_val = 0;
		ret = stg_config_u32_read(i+20, &stg_u32_val);
		if (ret != 0) 
		{
			LOG_ERR("STG(u32) Failed to read id %d(+20), err %d", i, ret);
		}
		LOG_INF("EEPvsSTG(u32): Id = %d(+20), EEP = %d, STG = %d", i, eep_u32_val, stg_u32_val);
	}

	/* Host port */
	char buff[24];
	ret = eep_read_host_port(&buff[0], 24);
	if (ret != 0)
	{
		LOG_ERR("EEP Failed to read host port");
	}

	char port[24];
	uint8_t len = 0;
	ret = stg_config_str_read(STG_STR_HOST_PORT, port, &len);
	if (ret != 0)
	{
		LOG_ERR("STG Failed to read host port");
	}
	LOG_INF("EEPvsSTG(Host port): EEP = %s, STG = %s", buff, port);

	uint8_t eep_sec_key[8];
	memset(eep_sec_key, 0, sizeof(eep_sec_key));
	ret = eep_read_ble_sec_key(eep_sec_key, sizeof(eep_sec_key));
	if (ret != 0)
	{
		LOG_ERR("EEP Failed to read BLE sec key");
	}

	char key[8];
	uint8_t key_len = 0;
	ret = stg_config_str_read(STG_STR_BLE_KEY, key, &key_len);
	if (ret != 0)
	{
		LOG_ERR("STG Failed to read host port");
	}
	LOG_INF("EEPvsSTG(Host port): EEP = %s, STG = %s", eep_sec_key, key);
}
