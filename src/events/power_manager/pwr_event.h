/*
 * Copyright (c) 2021 Nofence AS
 */

#ifndef _PWR_EVENT_H_
#define _PWR_EVENT_H_

#include "event_manager.h"
#include <zephyr.h>

/** @brief Enum for different electric pulse status */
enum pwr_state_flag {
	PWR_NORMAL = 0,
	PWR_LOW,
	PWR_CRITICAL,
	PWR_BATTERY,
	PWR_CHARGING
};

/** @brief Type for reboot reason from soft reset */
typedef enum {
	REBOOT_UNKNOWN = 0,
	REBOOT_NO_REASON,
	REBOOT_BLE_RESET,
	REBOOT_SERVER_RESET,
	REBOOT_FOTA_RESET,
	REBOOT_FATAL_ERR,
	REBOOT_REASON_CNT
}pwr_reboot_reason_t;

/** @brief Struct containg status messages regarding electric pulse events. */
struct pwr_status_event {
	struct event_header header;
	enum pwr_state_flag pwr_state;
	uint16_t battery_mv;
	uint16_t battery_mv_max;
	uint16_t battery_mv_min;
	uint16_t charging_ma;
};
EVENT_TYPE_DECLARE(pwr_status_event);

struct request_pwr_battery_event {
	struct event_header header;
};
EVENT_TYPE_DECLARE(request_pwr_battery_event);

struct request_pwr_charging_event {
	struct event_header header;
};
EVENT_TYPE_DECLARE(request_pwr_charging_event);

/** 
 * @brief Event to notify modules that need to shut down before SYS_REBOOT call 
 * 	to shut down gracefully if needed.
 * 
 * @details reboots_at k_uptime_get_32 + timer, telling when the system will 
 * reboot.
 */
struct pwr_reboot_event {
	struct event_header header;
	pwr_reboot_reason_t reason;
};
EVENT_TYPE_DECLARE(pwr_reboot_event);

#endif /* _PWR_EVENT_H_ */