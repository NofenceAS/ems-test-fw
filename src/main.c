
#include "ble/nf_ble.h"
#include "nf_eeprom.h"
#include <sys/printk.h>
#include <zephyr.h>
#include <event_manager.h>
#include <devicetree.h>
#include "fw_upgrade_events.h"
#include "fw_upgrade.h"
#include <logging/log.h>
#include "amc_handler.h"

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
	}
	/* Initialize firmware upgrade module. */
	if (fw_upgrade_module_init()) {
		LOG_ERR("Could not initialize firmware upgrade module");
	}
	/* Initialize animal monitor control module. */
	amc_module_init();
}
