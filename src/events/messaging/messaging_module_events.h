#include <event_manager.h>

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


