
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

#include "gnss.h"

#include "nf_version.h"

#include "fw_upgrade.h"

#define MODULE main
#include "module_state_event.h"

LOG_MODULE_REGISTER(MODULE, CONFIG_LOG_DEFAULT_LEVEL);

static int gnss_data_cb(const gnss_struct_t* data)
{
	LOG_ERR("Got GNSS data!");
	LOG_ERR("Lat=%d", data->lat);
	LOG_ERR("Lon=%d", data->lon);
	LOG_ERR("numSV=%d", data->num_sv);
	LOG_ERR("hAcc=%d", data->h_acc_dm);
	return 0;
}

static int gnss_lastfix_cb(const gnss_last_fix_struct_t* lastfix)
{
	LOG_ERR("Got GNSS LastFix data!");
	LOG_ERR("Unix timestamp is %d", (uint32_t)lastfix->unix_timestamp);
	return 0;
}

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

#if DT_NODE_HAS_STATUS(DT_ALIAS(gnss), okay)
	LOG_ERR("HAS GNSS!");
	const struct device *gnss_dev = DEVICE_DT_GET(DT_ALIAS(gnss));
	gnss_setup(gnss_dev, false);
	gnss_set_data_cb(gnss_dev, gnss_data_cb);
	gnss_set_lastfix_cb(gnss_dev, gnss_lastfix_cb);
	gnss_set_rate(gnss_dev, 1000);
	uint16_t rate = 0;
	gnss_get_rate(gnss_dev, &rate);
	LOG_ERR("GNSS rate=%d", rate);

	k_sleep(K_MSEC(10000));

	gnss_reset(gnss_dev, GNSS_RESET_MASK_COLD, GNSS_RESET_MODE_SW);
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
	/*if (ble_module_init()) {
		LOG_ERR("Could not initialize BLE module");
	}*/
	/* Initialize firmware upgrade module. */
	if (fw_upgrade_module_init()) {
		LOG_ERR("Could not initialize firmware upgrade module");
	}
	if (ep_module_init()) {
		LOG_ERR("Could not initialize electric pulse module");
	}
	/* Initialize animal monitor control module. */
	amc_module_init();

	/* Once EVERYTHING is initialized correctly and we get connection to
	* server, we can mark the image as valid. If we do not mark it as valid,
	* it will revert to the previous version on the next reboot that occurs.
	*/
	mark_new_application_as_valid();

	LOG_INF("Marked application firmware version %i as valid.",
		NF_X25_VERSION_NUMBER);
}
