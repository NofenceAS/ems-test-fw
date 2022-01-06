
#include "ble/nf_ble.h"
#include <sys/printk.h>
#include <zephyr.h>
#include <event_manager.h>
#include "fw_upgrade_events.h"
#include "fw_upgrade.h"
#include <logging/log.h>

#define LOG_MODULE_NAME main_app
LOG_MODULE_REGISTER(LOG_MODULE_NAME, CONFIG_LOG_DEFAULT_LEVEL);

/**
 * The Nofence X3 main entry point. This is
 * called from the Zephyr kernel.
 */
void main(void)
{
	LOG_INF("Starting nofence application");
	if (event_manager_init()) {
		LOG_ERR("Event manager could not initialize.");
	}
	nf_ble_init();
	/* Initialize firmware upgrade module. */
	if (fw_upgrade_module_init()) {
		LOG_ERR("Could not initialize firmware upgrade module");
	}
}
