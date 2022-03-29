
#include <devicetree.h>
#include <event_manager.h>
#include <logging/log.h>
#include <sys/printk.h>
#include <zephyr.h>

#include "diagnostics.h"
#include "fw_upgrade.h"
#include "fw_upgrade_events.h"
#include "nf_eeprom.h"
#include "ble_controller.h"
#include "cellular_controller.h"
#include "ep_module.h"
#include "amc_handler.h"
#include "nf_eeprom.h"
#include "buzzer.h"
#include "pwr_module.h"

#include "messaging.h"
#include "cellular_controller.h"

#include "gnss_controller.h"

#include "storage.h"
#include "nf_version.h"

#include "env_sensor_event.h"

#define MODULE main
#include "module_state_event.h"

LOG_MODULE_REGISTER(MODULE, CONFIG_LOG_DEFAULT_LEVEL);

/**
 * The Nofence X3 main entry point. This is
 * called from the Zephyr kernel.
 */
void main(void)
{
	LOG_INF("Starting Nofence application...");
	int err = stg_init_storage_controller();
	if (err) {
		LOG_ERR("Could not initialize storage controller, %i", err);
		return;
	}

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
	err = diagnostics_module_init();
	if (err) {
		LOG_ERR("Could not initialize diagnostics module %d", err);
	}
#endif

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

	err = ep_module_init();
	if (err) {
		LOG_ERR("Could not initialize electric pulse module. %d", err);
	}

	/* Initialize the power manager module. */
	err = pwr_module_init();
	if (err) {
		LOG_ERR("Could not initialize the power module %i", err);
	}

	err = buzzer_module_init();
	if (err) {
		LOG_ERR("Could not initialize buzzer module. %d", err);
	}

	/* Initialize animal monitor control module, depends on storage
	 * controller to be initialized first since amc sends
	 * a request for pasture data on init. 
	 */
	err = amc_module_init();
	if (err) {
		LOG_ERR("Could not initialize AMC module. %d", err);
	}

	/* Play welcome sound. */
	struct sound_event *sound_ev = new_sound_event();
	sound_ev->type = SND_WELCOME;
	EVENT_SUBMIT(sound_ev);

	err = cellular_controller_init();
	if (err) {
		LOG_ERR("Could not initialize cellular controller. %d", err);
	}

	err = messaging_module_init();
	if (err) {
		LOG_ERR("Could not initialize messaging module. %d", err);
	}

	err = gnss_controller_init();
	if (err) {
		LOG_ERR("Could not initialize GNSS controller. %d", err);
	}

	/* Once EVERYTHING is initialized correctly and we get connection to
	 * server, we can mark the image as valid. If we do not mark it as valid,
	 * it will revert to the previous version on the next reboot that occurs.
	 */
	mark_new_application_as_valid();

	LOG_INF("Booted application firmware version %i, and marked it as valid.",
		NF_X25_VERSION_NUMBER);
}
