#include <event_manager.h>
#include "nf_eeprom.h"
#include "collar_protocol.h"

/** @brief Empty event published by the messaging module to acknowledge
 * reception of proto_in messages from the cellular controller. */

struct messaging_ack_event {
	struct event_header header;
};

EVENT_TYPE_DECLARE(messaging_ack_event);

/** @brief Empty event published by the messaging module to notify
 * the cellular controller to terminate the connection and put the modem to
 * sleep
 * . */

struct messaging_stop_connection_event {
	struct event_header header;
};

EVENT_TYPE_DECLARE(messaging_stop_connection_event);

/** @brief outbound proto messages to be sent to the server (binary format).
 * Published by the messaging module and consumed by the cellular controller.
 * */

struct messaging_proto_out_event {
	struct event_header header;
	uint8_t *buf;
	size_t len;
};

EVENT_TYPE_DECLARE(messaging_proto_out_event);

/** @brief notify cellular_controller of a new host address.
 * Published by the messaging module and consumed by the cellular controller.
 * */

struct messaging_host_address_event {
	struct event_header header;
	char address[EEP_HOST_PORT_BUF_SIZE];
};

EVENT_TYPE_DECLARE(messaging_host_address_event);



struct update_collar_mode {
	struct event_header header;
	Mode collar_mode;
};

EVENT_TYPE_DECLARE(update_collar_mode);

struct update_collar_status {
	struct event_header header;
	CollarStatus collar_status;
};

EVENT_TYPE_DECLARE(update_collar_status);

struct update_fence_status {
	struct event_header header;
	FenceStatus fence_status;
};

EVENT_TYPE_DECLARE(update_fence_status);

struct update_fence_version {
	struct event_header header;
	uint32_t fence_version;
};

EVENT_TYPE_DECLARE(update_fence_version);

struct update_flash_erase {
	struct event_header header;
};

EVENT_TYPE_DECLARE(update_flash_erase);

struct update_zap_count {
	struct event_header header;
	uint16_t count;
};

EVENT_TYPE_DECLARE(update_zap_count);