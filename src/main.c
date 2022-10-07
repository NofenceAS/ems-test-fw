
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

/* NVS Test Components */
// #include <drivers/flash.h>
// #include <storage/flash_map.h>
// #include <fs/nvs.h>

// static const struct device* mp_device;
// static const struct flash_area* mp_flash_area;
// static struct nvs_fs m_file_system;

// struct k_work_delayable nvs_read_flash_work;
// // struct k_work_delayable nvs_write_flash_work;

// static struct nvs_fs m_fs;
// static const struct flash_area *m_area12;
// #define TEST_1_ID 1
// #define TEST_2_ID 2

// void nvs_read_flash_fn()
// {
// 	printk("NVS Read flash work");

// 	char buff[16];

// 	int rc = nvs_read(&m_fs, TEST_1_ID, &buff, sizeof(buff));
// 	if (rc > 0) 
// 	{
// 		printk("Id: %d, Address: %s\n", TEST_1_ID, buff);
// 	} 
// 	else
// 	{
// 		printk("No address found at id %d\n", TEST_1_ID);
// 	}

// 	uint32_t cnt_read = 0;
// 	rc = nvs_read(&m_fs, TEST_2_ID, &cnt_read, sizeof(cnt_read));
// 	if (rc != 0)
// 	{
// 		printk("Value read at id %d = %d\n", TEST_2_ID, cnt_read);
// 	}
// 	else
// 	{
// 		printk("No value found at id %d\n", TEST_2_ID);
// 	}
// }

// // void nvs_write_flash_fn()
// // {
// // 	printk("NVS Write flash work");
// // }

// /* End NVS Test Components */

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








    // int err12;
	// int flash_area_id;

	// flash_area_id = FLASH_AREA_ID(config_partition);

	// err12 = flash_area_open(flash_area_id, &mp_flash_area);
	// if (err12 != 0) 
    // {
    //     LOG_ERR("Unable to open config storage flash area");
	// 	return;
	// }

    // LOG_DBG("STG Config: AreaID(%d), FaID(%d), FaOff(%d), FaSize(%d)", 
    //     flash_area_id, 
    //     (uint8_t)mp_flash_area->fa_id, 
    //     (int)mp_flash_area->fa_off, 
    //     (int)mp_flash_area->fa_size);

	// mp_device = device_get_binding(mp_flash_area->fa_dev_name);
	// if (mp_device == NULL) 
    // {
	// 	LOG_ERR("Unable to get config storage device");
    //     return;
	// }

	// m_file_system.offset = (int)mp_flash_area->fa_off;
	// m_file_system.sector_size = 4096;
	// m_file_system.sector_count = 2;

	// err12 = nvs_init(&m_file_system, mp_device->name);
	// if (err12 != 0) 
    // {
	// 	LOG_ERR("Fail to initialize config storage");
    //     return;
	// }

	// uint8_t value_write = 1;
	// uint8_t value_read = 0;

    // err12 = nvs_write(&m_file_system, (uint16_t)STG_U32_WARN_CNT_TOT, &value_write, sizeof(uint8_t));
    // if (err12 < 0)
    // {
    //     LOG_ERR("STG u32 write, failed write to storage at id %d", 20);
    //     return;
    // }

    // err12 = nvs_read(&m_file_system, (uint16_t)STG_U32_WARN_CNT_TOT, &value_read, sizeof(uint8_t));
	// if (err12 < 0) 
	// {
	// 	LOG_ERR("STG u32 read, failed to read storage at id %d", 20);
    //     return;
	// } 
	// LOG_INF("STG read, value = %d", value_read);

	// value_write = 2;
	// err12 = nvs_write(&m_file_system, (uint16_t)STG_U32_WARN_CNT_TOT, &value_write, sizeof(uint8_t));
    // if (err12 < 0)
    // {
    //     LOG_ERR("STG u32 write, failed write to storage at id %d", 20);
    //     return;
    // }

    // err12 = nvs_read(&m_file_system, (uint16_t)STG_U32_WARN_CNT_TOT, &value_read, sizeof(uint8_t));
	// if (err12 < 0) 
	// {
	// 	LOG_ERR("STG u32 read, failed to read storage at id %d", 20);
    //     return;
	// } 
	// LOG_INF("STG read, value = %d", value_read);













	// err = stg_config_init();
	// if (err != 0)
	// {
	// 	LOG_ERR("STG Config failed to initialize");
	// }
	// else
	// {
	// 	LOG_INF("STG Config initialized");
	// }

	// uint8_t val = 0;

	// stg_config_u8_write(STG_U8_EMS_PROVIDER, 1);

	// stg_config_u8_read(STG_U8_EMS_PROVIDER, &val);
	// LOG_INF("STG Config: Id:%d, Value:%d", STG_U8_EMS_PROVIDER, val);

	// stg_config_u8_write(STG_U8_EMS_PROVIDER, 2);

	// stg_config_u8_read(STG_U8_EMS_PROVIDER, &val);
	// LOG_INF("STG Config: Id:%d, Value:%d", STG_U8_EMS_PROVIDER, val);

	// stg_config_u8_read(STG_U8_EMS_PROVIDER, &val);
	// LOG_INF("STG Config: Id:%d, Value:%d", STG_U8_EMS_PROVIDER, val);

	// uint32_t val = 0;

	//stg_config_u32_write(STG_U32_UID, 1);

	// stg_config_u32_read(STG_U32_UID, &val);
	// LOG_INF("STG Config: Id:%d, Value:%d", STG_U32_UID, val);

	// stg_config_u32_write(STG_U32_UID, 3);

	// stg_config_u32_read(STG_U32_UID, &val);
	// LOG_INF("STG Config: Id:%d, Value:%d", STG_U32_UID, val);

	// stg_config_u32_read(STG_U32_UID, &val);
	// LOG_INF("STG Config: Id:%d, Value:%d", STG_U32_UID, val);



	// /* NVS TEST */
	// //k_work_init_delayable(&nvs_read_flash_work, nvs_read_flash_fn);
	// // k_work_init_delayable(&nvs_write_flash_work, nvs_write_flash_fn);

	// const struct device *dev12;
	// const struct flash_area *area12;
	// int area12_id;
	// struct flash_pages_info info;
	// char buff[16];

	// area12_id = FLASH_AREA_ID(config_partition);
	// area12 = m_area12;

	// err = flash_area_open(area12_id, &area12);
	// if (err != 0) {
	// 	LOG_ERR("NVS error opening flash area");
	// 	return;
	// }

	// LOG_INF("NVS: AreaID(%d), FaID(%d), FaOff(%d), FaSize(%d)", 
	// 			area12_id, (uint8_t)area12->fa_id, (int)area12->fa_off, (int)area12->fa_size);

	// /* Check if area has a flash device available. */
	// dev12 = device_get_binding(area12->fa_dev_name);
	// if (dev12 == NULL) {
	// 	LOG_ERR("NVS could not get device");
	// }

	// flash_area_close(area12);



	// m_fs.offset = (int)area12->fa_off;
	// err = flash_get_page_info_by_offs(dev12, m_fs.offset, &info);
	// if (err != 0) {
	// 	LOG_ERR("NVS unable to get page info...");
	// 	return;
	// }

	// m_fs.sector_size = 4096;
	// m_fs.sector_count = 2;

	// err = nvs_init(&m_fs, dev12->name);
	// if (err != 0) {
	// 	LOG_ERR("Internal Flash Initialisation Failed");
	// }
	// else{
	// 	LOG_INF("Internal Flash Initialisation Passed");
	// }

	// int rc = nvs_read(&m_fs, TEST_1_ID, &buff, sizeof(buff));
	// if (rc > 0) 
	// {
	// 	LOG_INF("Id: %d, Address: %s", TEST_1_ID, buff);
	// } 
	// else  
	// {
	// 	strcpy(buff, "192.168.1.1");
	// 	LOG_INF("No address found, adding %s at id %d", buff, TEST_1_ID);
	// 	(void)nvs_write(&m_fs, TEST_1_ID, &buff, strlen(buff)+1);
	// }

	// rc = nvs_read(&m_fs, TEST_1_ID, &buff, sizeof(buff));
	// if (rc > 0) 
	// {
	// 	LOG_INF("Id: %d, Address: %s", TEST_1_ID, buff);
	// } 
	// else
	// {
	// 	LOG_ERR("Still no content");
	// }



	// uint32_t cnt_read = 0;
	// rc = nvs_read(&m_fs, TEST_2_ID, &cnt_read, sizeof(cnt_read));
	// if (rc != 0)
	// {
	// 	printk("Value read at id %d = %d\n", TEST_2_ID, cnt_read);
	// }
	// else
	// {
	// 	printk("No value found at id %d\n", TEST_2_ID);
	// }

	// k_work_schedule(&nvs_read_flash_work, K_SECONDS(30));


	// uint32_t cnt = 0;
	// uint32_t cnt_read = 0;
	// for (int i = 0; i < 1000; i++)
	// {
	// 	rc = nvs_read(&m_fs, TEST_2_ID, &cnt_read, sizeof(cnt_read));
	// 	if (rc != 0)
	// 	{
	// 		printk("Value read at id %d = %d\n", TEST_2_ID, cnt_read);
	// 	}
	// 	else
	// 	{
	// 		printk("No value found at id %d\n", TEST_2_ID);
	// 	}

	// 	printk("Writing at id %d, value = %d\n", TEST_2_ID, cnt);
	// 	(void)nvs_write(&m_fs, TEST_2_ID, &cnt, sizeof(cnt));

	// 	cnt++;
	// 	k_sleep(K_MSEC(100));
	// }

	// rc = nvs_read(&m_fs, TEST_1_ID, &buf, sizeof(buf));
	// if (rc > 0) { /* item was found, show it */
	// 	printk("Id: %d, Address: %s\n", TEST_1_ID, buf);
	// }


	// k_sleep(K_SECONDS(5));
	// return;

	/* END NVS TEST */

}
