
#include "ble/nf_ble.h"
#include <sys/printk.h>
#include <zephyr.h>

/**
 * The Nofence X3 main entry point. This is
 * called from the Zephyr kernel.
 */
void main(void)
{
	printk("main %p\n", k_current_get());
	nf_ble_init();
}