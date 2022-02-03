
#include <devicetree.h>
#include <event_manager.h>
#include <logging/log.h>
#include <sys/printk.h>
#include <zephyr.h>

#include "ble/nf_ble.h"
#include "fw_upgrade.h"
#include "fw_upgrade_events.h"
#include "nf_eeprom.h"
#include "ble_controller.h"
#include "amc_handler.h"
#include "nf_eeprom.h"

#include "nf_version.h"

#include <dfu/mcuboot.h>
#include <dfu/dfu_target_mcuboot.h>
#include <net/fota_download.h>

#define MODULE main
#include "module_state_event.h"

LOG_MODULE_REGISTER(MODULE, CONFIG_LOG_DEFAULT_LEVEL);

static void fota_dl_handler(const struct fota_download_evt *evt)
{
	switch (evt->id) {
	case FOTA_DOWNLOAD_EVT_ERROR:
		LOG_ERR("Received error from fota_download");
		/* Fallthrough */
	case FOTA_DOWNLOAD_EVT_FINISHED:
		LOG_INF("Fota download finished.");
		break;

	default:
		break;
	}
}

/**
 * The Nofence X3 main entry point. This is
 * called from the Zephyr kernel.
 */
void main(void)
{
	LOG_INF("Starting Nofence application with version %i",
		NF_X25_VERSION_NUMBER);

	const struct device *eeprom_dev = DEVICE_DT_GET(DT_ALIAS(eeprom));
	eep_init(eeprom_dev);

	/* Initialize the event manager. */
	if (event_manager_init()) {
		LOG_ERR("Event manager could not initialize.");
	}
	/* Initialize BLE module. */
	if (ble_module_init()) {
		LOG_ERR("Could not initialize BLE module");
	}
	/* Initialize firmware upgrade module. */
	if (fw_upgrade_module_init()) {
		LOG_ERR("Could not initialize firmware upgrade module");
	}
	/* Initialize animal monitor control module. */
	amc_module_init();

	if (NF_X25_VERSION_NUMBER < 2001) {
		int err = fota_download_init(fota_dl_handler);
		if (err) {
			LOG_ERR("fota_download_init() failed, err %d", err);
		}

		err = fota_download_start(CONFIG_DOWNLOAD_HOST,
					  CONFIG_DOWNLOAD_FILE, -1, 0, 0);
		if (err) {
			LOG_ERR("fota_download_start() failed, err %d", err);
		} else {
			boot_write_img_confirmed();
		}
	}
}