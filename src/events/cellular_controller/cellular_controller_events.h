#include <event_manager.h>
#include <modem_nf.h>

typedef enum {
	POWER_OFF = 0,
	POWER_ON = 1,
	SLEEP = 2,
} modem_pwr_mode;

/** @brief Empty event published by the cellular controller to acknowledge
 * reception of proto_out messages from the messaging module. */

struct cellular_ack_event {
	struct event_header header;
};

EVENT_TYPE_DECLARE(cellular_ack_event);

/** @brief inbound proto messages received from the server (binary format).
 * Published by the cellular controller and consumed by the messaging
 * module. */

struct cellular_proto_in_event {
	struct event_header header;
	uint8_t *buf;
	size_t len;
};

EVENT_TYPE_DECLARE(cellular_proto_in_event);

enum cause {
	OTHER = 0,
	SOCKET_OPEN = 1,
	SOCKET_CLOSE = 2,
	SOCKET_CONNECT = 3,
	SOCKET_SEND = 4,
	SOCKET_RECV = 5,
	CONNECTION_LOST = 6,
};

/** @brief Error event published by the cellular controller when it
 * fails to send a message to the server. */

struct cellular_error_event {
	struct event_header header;
	enum cause cause;
	int8_t err_code;
};

EVENT_TYPE_DECLARE(cellular_error_event);

/** @brief Event published by the cellular controller in reply to
 * check_connection event. */

struct connection_state_event {
	struct event_header header;
	bool state;
};

EVENT_TYPE_DECLARE(connection_state_event);

struct modem_state {
	struct event_header header;
	modem_pwr_mode mode;
};
EVENT_TYPE_DECLARE(modem_state);

/** @brief Event published by the send message thread to free the allocated
 * message ram in cellular_controller event handler.
 * */

struct free_message_mem_event {
	struct event_header header;
};
EVENT_TYPE_DECLARE(free_message_mem_event);

struct gsm_info_event {
	struct event_header header;
	struct gsm_info gsm_info;
};
EVENT_TYPE_DECLARE(gsm_info_event);