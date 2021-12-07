/*
 * Copyright (c) 2021 Nofence AS
 */

#ifndef _STORAGE_EVENT_H_
#define _STORAGE_EVENT_H_

/**
 * @brief Storage Event
 * @defgroup storage_event Storage Event
 * @{
 */
#include "event_manager.h"

#ifdef __cplusplus
extern "C" {
#endif


/** @brief Storage event types. */
enum storage_event_type {
	STORAGE_EVT_READ_SERIAL_NR,
	STORAGE_EVT_WRITE_SERIAL_NR,
};

struct storage_pvt {
    uint32_t serial_number;       // i.e: 123456
    uint8_t server_ip_address[4]; // i.e: 255.255.255.255
    uint8_t firmware_version[3];  // i.e: 1.2.3
};

/** @brief Storage event. */
struct storage_event {
	struct event_header header;
    enum storage_event_type type;
	union {
        struct storage_pvt pvt;
        int err;
    } data;
};

EVENT_TYPE_DECLARE(storage_event);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _STORAGE_EVENT_H_ */
