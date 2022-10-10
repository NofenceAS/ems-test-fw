
#include <devicetree.h>
#include <event_manager.h>
#include <logging/log.h>
#include <sys/printk.h>
#include <zephyr.h>

#include "error_event.h"

#include "diagnostics.h"
#include "selftest.h"
#include "fw_upgrade.h"
#include "fw_upgrade_events.h"
#include "stg_config.h" //NVS
#include "nf_settings.h"
#include "ble_controller.h"
#include "cellular_controller.h"
#include "ep_module.h"
#include "amc_handler.h"
#include "nf_settings.h"
#include "buzzer.h"
#include "pwr_module.h"
#include "pwr_event.h"
/* For reboot reason. */
#include <nrf.h>

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

/* Fetched from 52840 datasheet RESETREAS register. */
#define SOFT_RESET_REASON_FLAG (1 << 2)

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
	err = eep_uint32_read(EEP_UID, &serial_id);
	if (serial_id) {
		LOG_INF("Device Serial Number stored in EEPROM: %d", serial_id);
	} else {
		LOG_WRN("Missing device Serial Number in EEPROM");
	}
#endif

	err = stg_config_init();
	if (err != 0) {
		LOG_ERR("STG Config failed to initialize");
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

	err = stg_fcb_reset_and_init();
	if (err != 0) {
		LOG_ERR("Failed to reset storage controller (%d)", err);
	}

	//err = stg_init_storage_controller();
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










	// /* UID */
	// uint32_t serial = 0;
	// ret = eep_uint32_read(EEP_UID, &serial);
	// if (ret != 0) { LOG_ERR("Failed to read eep serial number"); }
	// LOG_INF("Serial number = %d", serial);

	// ret = stg_config_u32_write(STG_U32_UID, serial);
	// if (ret != 0) { LOG_ERR("Failed to write serial number"); }

	// /* EMS */
	// uint8_t ems_provider = 0;
	// ret = eep_uint8_read(EEP_EMS_PROVIDER, &ems_provider);
	// if (ret != 0) { LOG_ERR("Failed to read EMS provider"); }
	// LOG_INF("EMS provider = %d", ems_provider);

	// ret = stg_config_u8_write(STG_U8_EMS_PROVIDER, ems_provider);
	// if (ret != 0) { LOG_ERR("Failed to write EMS provider"); }

	// /* Prod. type */
	// uint16_t prod_type = 0;
	// ret = eep_uint16_read(EEP_PRODUCT_TYPE, &prod_type);
	// if (ret != 0) { LOG_ERR("Failed to read product type"); }
	// LOG_INF("Product type = %d", prod_type);

	// ret = stg_config_u16_write(STG_U16_PRODUCT_TYPE, prod_type);
	// if (ret != 0) { LOG_ERR("Failed to write product type"); }

	// /* HW version */
	// uint8_t hw_ver = 0;
	// ret = eep_uint8_read(EEP_HW_VERSION, &hw_ver);
	// if (ret != 0) { LOG_ERR("Failed to read hw version"); }
	// LOG_INF("HW version = %d", hw_ver);

	// ret = stg_config_u8_write(STG_U8_HW_VERSION, hw_ver);
	// if (ret != 0) { LOG_ERR("Failed to write EMS provider"); }

	// /* BOM PCB REV */
	// uint8_t bom_pcb_rev = 0;
	// ret = eep_uint8_read(EEP_BOM_PCB_REV, &bom_pcb_rev);
	// if (ret != 0) { LOG_ERR("Failed to read pcb rev"); }
	// LOG_INF("BOM PCB Rev = %d", bom_pcb_rev);

	// ret = stg_config_u8_write(STG_U8_BOM_PCB_REV, bom_pcb_rev);
	// if (ret != 0) { LOG_ERR("Failed to write EMS provider"); }

	// /* BOM MEC REV */
	// uint8_t bom_mec_rev = 0;
	// ret = eep_uint8_read(EEP_BOM_MEC_REV, &bom_mec_rev);
	// if (ret != 0) { LOG_ERR("Failed to write mec rev"); }
	// LOG_INF("BOM MEC Rev = %d", bom_mec_rev);

	// ret = stg_config_u8_write(STG_U8_BOM_MEC_REV, bom_mec_rev);
	// if (ret != 0) { LOG_ERR("Failed to write EMS provider"); }

	// /* Product Record Rev */
	// uint8_t product_record_rev = 0;
	// ret = eep_uint8_read(EEP_PRODUCT_RECORD_REV, &product_record_rev);
	// if (ret != 0) { LOG_ERR("Failed to write product record rev"); }

	// ret = stg_config_u8_write(STG_U8_PRODUCT_RECORD_REV, product_record_rev);
	// if (ret != 0) { LOG_ERR("Failed to write EMS provider"); }

	// /* Host port */
	// char buf[24];
	// ret = eep_read_host_port(&buf[0], 24);
	// if (ret != 0) { LOG_ERR("Failed to read host port"); }
	// LOG_INF("Host port = %s", buf);

	// ret = stg_config_str_write(STG_STR_HOST_PORT, buf, STG_CONFIG_HOST_PORT_BUF_LEN-1);
	// if (ret != 0) { LOG_ERR("Failed to read host port number"); }


	// /* Readback */
	// uint32_t serial_rb = 0;
	// ret = stg_config_u32_read(STG_U32_UID, &serial_rb);
	// if (ret != 0) { LOG_ERR("Failed to read serial number"); }
	// LOG_INF("STG Serial no = %d", serial_rb);

	// /* EMS */
	// uint8_t ems_provider_rb = 0;
	// ret = stg_config_u8_read(STG_U8_EMS_PROVIDER, &ems_provider_rb);
	// if (ret != 0) { LOG_ERR("Failed to read EMS provider"); }
	// LOG_INF("STG EMS = %d", ems_provider_rb);

	// /* Prod. type */
	// uint16_t prod_type_rb = 0;
	// ret = stg_config_u16_read(STG_U16_PRODUCT_TYPE, &prod_type_rb);
	// if (ret != 0) { LOG_ERR("Failed to read product type"); }
	// LOG_INF("STG Prod type = %d", prod_type_rb);

	// /* HW version */
	// uint8_t hw_ver_rb = 0;
	// ret = stg_config_u8_read(STG_U8_HW_VERSION, &hw_ver_rb);
	// if (ret != 0) { LOG_ERR("Failed to read EMS provider"); }
	// LOG_INF("STG HW Ver = %d", hw_ver_rb);

	// /* BOM PCB REV */
	// uint8_t bom_pcb_rev_rb = 0;
	// ret = stg_config_u8_read(STG_U8_BOM_PCB_REV, &bom_pcb_rev_rb);
	// if (ret != 0) { LOG_ERR("Failed to read EMS provider"); }
	// LOG_INF("STG PCB Rev = %d", bom_pcb_rev_rb);

	// /* BOM MEC REV */
	// uint8_t bom_mec_rev_rb = 0;
	// ret = stg_config_u8_read(STG_U8_BOM_MEC_REV, &bom_mec_rev_rb);
	// if (ret != 0) { LOG_ERR("Failed to read EMS provider"); }
	// LOG_INF("STG MEC Rev = %d", bom_mec_rev_rb);

	// /* Product Record Rev */
	// uint8_t product_record_rev_rb = 0;
	// ret = stg_config_u8_read(STG_U8_PRODUCT_RECORD_REV, &product_record_rev_rb);
	// if (ret != 0) { LOG_ERR("Failed to read EMS provider"); }
	// LOG_INF("STG Record Rev = %d", product_record_rev_rb);

	// /* Host port */
	// char port[STG_CONFIG_HOST_PORT_BUF_LEN];
	// uint8_t port_len = 0;
	// ret = stg_config_str_read(STG_STR_HOST_PORT, port, &port_len);
	// if (ret != 0) { LOG_ERR("Failed to read host port"); }
	// LOG_INF("STG Host port = (%d)%s", port_len, port);




	// uint8_t val = 0;

	// stg_config_u8_write(STG_U8_EMS_PROVIDER, 1);

	// stg_config_u8_read(STG_U8_EMS_PROVIDER, &val);
	// // LOG_INF("STG Config: Id:%d, Value:%d", STG_U8_EMS_PROVIDER, val);

	// stg_config_u8_write(STG_U8_EMS_PROVIDER, 2);

	// stg_config_u8_read(STG_U8_EMS_PROVIDER, &val);
	// // LOG_INF("STG Config: Id:%d, Value:%d", STG_U8_EMS_PROVIDER, val);

	// stg_config_u8_read(STG_U8_EMS_PROVIDER, &val);
	// LOG_INF("STG Config: Id:%d, Value:%d", STG_U8_EMS_PROVIDER, val);



	// char port[STG_CONFIG_HOST_PORT_BUF_LEN];
	// memset(port, 0, STG_CONFIG_HOST_PORT_BUF_LEN);
	// strcpy(port, "1.1.1.2\0");

	// err = stg_config_str_write(STG_STR_HOST_PORT, port, strlen(port));
	// if (err != 0)
	// {
	// 	LOG_ERR("Shoit");
	// }

	// char port_read[STG_CONFIG_HOST_PORT_BUF_LEN]; 
	// uint8_t port_read_length = 0;
	// err = stg_config_str_read(STG_STR_HOST_PORT, port_read, &port_read_length);
	// if (err != 0)
	// {
	// 	LOG_ERR("Shoit 1");
	// }
	// else
	// {
	// 	LOG_INF("Port: Len=%d, buff=%s", port_read_length, port_read);
	// }

	// err = stg_config_str_read(STG_STR_HOST_PORT, port_read, &port_read_length);
	// if (err != 0)
	// {
	// 	LOG_ERR("Shoit 1");
	// }
	// else
	// {
	// 	LOG_INF("Port: Len=%d, buff=%s", port_read_length, port_read);
	// }
}
