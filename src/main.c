
#include <devicetree.h>
#include <event_manager.h>
#include <logging/log.h>
#include <sys/printk.h>
#include <zephyr.h>

#include "ble/nf_ble.h"
#include "fw_upgrade.h"
#include "cellular_controller.h"
#include "messaging.h"
#include "fw_upgrade_events.h"
#include "nf_eeprom.h"
#include "ble_controller.h"
#include "ep_module.h"
#include "amc_handler.h"
#include "nf_eeprom.h"

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
/* Not all boards have eeprom */
#if DT_NODE_HAS_STATUS(DT_ALIAS(eeprom), okay)
	const struct device *eeprom_dev = DEVICE_DT_GET(DT_ALIAS(eeprom));
	eep_init(eeprom_dev);
#endif
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
	if (ep_module_init()) {
		LOG_ERR("Could not initialize electric pulse module");
	}
	/* Initialize animal monitor control module. */
	amc_module_init();
	cellular_controller_init();
	messaging_module_init();
}
