
#include <devicetree.h>
#include <event_manager.h>
#include <logging/log.h>
#include <sys/printk.h>
#include <zephyr.h>

#include "diagnostics.h"
#include "ble/nf_ble.h"
#include "fw_upgrade.h"
#include "fw_upgrade_events.h"
#include "nf_eeprom.h"
#include "ble_controller.h"

#define MODULE main
#include "module_state_event.h"

LOG_MODULE_REGISTER(MODULE, CONFIG_LOG_DEFAULT_LEVEL);

/**
 * The Nofence X3 main entry point. This is
 * called from the Zephyr kernel.
 */
void main(void)
{
	LOG_INF("Starting Nofence application");
	const struct device *eeprom_dev = DEVICE_DT_GET(DT_ALIAS(eeprom));
	if (eeprom_dev == NULL) {
		LOG_ERR("No EEPROM detected!");
	} else {
		LOG_ERR("EEPROM initialized!");
	}
	eep_init(eeprom_dev);

	/* Initialize diagnostics module. */
#if CONFIG_DIAGNOSTICS
	if (diagnostics_module_init()) {
		LOG_ERR("Could not initialize diagnostics module");
	}
#endif
	/* Initialize the event manager. */
	if (event_manager_init()) {
		LOG_ERR("Event manager could not initialize.");
	}
	/* Initialize BLE module. */
	/*if (ble_module_init()) {
		LOG_ERR("Could not initialize BLE module");
	}*/
	/* Initialize firmware upgrade module. */
	if (fw_upgrade_module_init()) {
		LOG_ERR("Could not initialize firmware upgrade module");
	}

	
}
