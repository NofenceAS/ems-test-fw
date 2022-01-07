
#include "ble/nf_ble.h"
#include "nf_eeprom.h"
#include <sys/printk.h>
#include <zephyr.h>
#include <event_manager.h>
#include <devicetree.h>

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
	/* Initialize the event manager to have modules subscribed to respective topics */
	int err = event_manager_init();
	if (err) {
		// Error handler
	}
}