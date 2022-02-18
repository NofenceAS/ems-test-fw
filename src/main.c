
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
#include "ep_module.h"
#include "amc_handler.h"
#include "nf_eeprom.h"
#include "buzzer.h"

#include "nf_version.h"

#include "fw_upgrade.h"

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
	if (eeprom_dev == NULL) {
		LOG_ERR("No EEPROM detected!");
	}
	eep_init(eeprom_dev);
#endif

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
	if (ble_module_init()) {
		LOG_ERR("Could not initialize BLE module");
	}
	if (ep_module_init()) {
		LOG_ERR("Could not initialize electric pulse module");
	}
	/* Initialize animal monitor control module. */
	amc_module_init();

	init_sound_controller();

	/* Once EVERYTHING is initialized correctly and we get connection to
	* server, we can mark the image as valid. If we do not mark it as valid,
	* it will revert to the previous version on the next reboot that occurs.
	*/
	mark_new_application_as_valid();

	LOG_INF("Marked application firmware version %i as valid.",
		NF_X25_VERSION_NUMBER);
}
