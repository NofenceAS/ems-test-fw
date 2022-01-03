
#include "ble/nf_ble.h"
#include <sys/printk.h>
#include <zephyr.h>
#include <logging/log.h>
#include <event_manager.h>

#define MODULE main
#include "module_state_event.h"

LOG_MODULE_REGISTER(MODULE);

/**
 * The Nofence X3 main entry point. This is
 * called from the Zephyr kernel.
 */
void main(void)
{
	LOG_INF("Starting nofence application");
	if (event_manager_init()) {
		LOG_ERR("Event manager not initialized");
	} else {
		module_set_state(MODULE_STATE_READY);
	}
}