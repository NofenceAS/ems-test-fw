/*
 * Copyright (c) 2021 Nofence AS
 */

#ifndef _FW_UPGRADE_EVENTS_H_
#define _FW_UPGRADE_EVENTS_H_

#include "event_manager.h"
#include <zephyr.h>

/** @brief Enum for different dfu statuses, 
 *         if we want modules to shutdown correctly etc...
 *         We call it schedule reboot, since fw_upgrade module must schedule 
 *         a reboot after n seconds when done.
 */
enum dfu_status_flag {
	DFU_STATUS_IDLE = 0,
	DFU_STATUS_IN_PROGRESS = 1,
	DFU_STATUS_SUCCESS_REBOOT_SCHEDULED = 2
};

/** @brief Struct containg status messages regarding the firmware upgrade. */
struct dfu_status_event {
	struct event_header header;

	enum dfu_status_flag dfu_status;

	/** If an error occurs this is not 0. */
	int dfu_error;
};

EVENT_TYPE_DECLARE(dfu_status_event);

#endif /* _FW_UPGRADE_EVENTS_H_ */