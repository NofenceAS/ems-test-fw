
#include "ble/nf_ble.h"
#include "nf_eeprom.h"
#include <sys/printk.h>
#include <zephyr.h>
#include <event_manager.h>
#include <devicetree.h>
#include "fw_upgrade.h"
#include <logging/log.h>
#include

#define LOG_MODULE_NAME main_app
LOG_MODULE_REGISTER(LOG_MODULE_NAME, CONFIG_LOG_DEFAULT_LEVEL);

/**
 * The Nofence X3 main entry point. This is
 * called from the Zephyr kernel.
 */
void main(void)
{
	printk("main %p\n", k_current_get());
	nf_ble_init();
	const struct device *eeprom_dev = DEVICE_DT_GET(DT_ALIAS(eeprom));
	eep_init(eeprom_dev);

	/* Initialize the event manager. */
	if (event_manager_init()) {
		LOG_ERR("Event manager could not initialize.");
	} else {
		// Initialize storage modules
		//struct config_event *event_1 = new_config_event();
		//event_1->type = CONFIG_EVT_START;
		//EVENT_SUBMIT(event_1);
		//
		//k_sleep(K_MSEC(1000));
		//
		//// Write serial number to external flash
		//struct storage_event *event_2 = new_storage_event();
		//event_2->type = STORAGE_EVT_WRITE_SERIAL_NR;
		//event_2->data.pvt.serial_number = 48995;
		//EVENT_SUBMIT(event_2);
		//
		//k_sleep(K_MSEC(1000));
		//
		//// Read serial_number from external flash
		//struct storage_event *event_3 = new_storage_event();
		//event_3->type = STORAGE_EVT_READ_SERIAL_NR;
		//EVENT_SUBMIT(event_3);
		//
		//k_sleep(K_MSEC(1000));
		//
		//// Write fw_version to external flash
		//struct storage_event *event_4 = new_storage_event();
		//event_4->type = STORAGE_EVT_WRITE_FW_VERSION;
		//uint8_t dummy_fw_version[3] = { 0x00, 0x01, 0x02 };
		//memcpy(event_4->data.pvt.firmware_version, dummy_fw_version,
		//       sizeof(dummy_fw_version));
		//EVENT_SUBMIT(event_4);
		//
		//k_sleep(K_MSEC(1000));
		//
		//// Read fw_version from external flash
		//struct storage_event *event_5 = new_storage_event();
		//event_5->type = STORAGE_EVT_READ_FW_VERSION;
		//EVENT_SUBMIT(event_5);
		//
		//k_sleep(K_MSEC(1000));
		//
		//// Write ip-address to external flash
		//struct storage_event *event_6 = new_storage_event();
		//event_6->type = STORAGE_EVT_WRITE_IP_ADDRESS;
		//uint8_t dummy_ip_address[4] = { 0xff, 0xff, 0xff, 0xff };
		//memcpy(event_6->data.pvt.server_ip_address, dummy_ip_address,
		//       sizeof(dummy_ip_address));
		//EVENT_SUBMIT(event_6);
		//
		//k_sleep(K_MSEC(1000));
		//
		//// Read ip-address from external flash
		//struct storage_event *event_7 = new_storage_event();
		//event_7->type = STORAGE_EVT_READ_IP_ADDRESS;
		//EVENT_SUBMIT(event_7);
	}
	/* Initialize firmware upgrade module. */
	if (fw_upgrade_module_init()) {
		LOG_ERR("Could not initialize firmware upgrade module");
	}
}
