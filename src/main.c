
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

#include "log_structure.h"
#include "ano_structure.h"
#include "pasture_structure.h"
#include "storage_events.h"
#include "storage.h"

#define MODULE main
#include "module_state_event.h"

LOG_MODULE_REGISTER(MODULE, CONFIG_LOG_DEFAULT_LEVEL);

/**
 * The Nofence X3 main entry point. This is
 * called from the Zephyr kernel.
 */
void main(void)
{
	int err = init_storage_controller();
	if (err) {
		LOG_ERR("Could not initialize storage controller, %i", err);
		return;
	}
	LOG_INF("Starting Nofence application");
	const struct device *eeprom_dev = DEVICE_DT_GET(DT_ALIAS(eeprom));
	eep_init(eeprom_dev);

	/* Initialize the event manager. */
	err = event_manager_init();
	if (err) {
		LOG_ERR("Event manager could not initialize. %d", err);
	}
	/* Initialize BLE module. */
	err = ble_module_init();
	if (err) {
		LOG_ERR("Could not initialize BLE module. %d", err);
	}
	/* Initialize firmware upgrade module. */
	err = fw_upgrade_module_init();
	if (err) {
		LOG_ERR("Could not initialize firmware upgrade module. %d",
			err);
	}

	/* Initialize animal monitor control module, depends on storage
	 * controller to be initialized first since amc sends
	 * a request for pasture data on init. 
	 */
	amc_module_init();
}