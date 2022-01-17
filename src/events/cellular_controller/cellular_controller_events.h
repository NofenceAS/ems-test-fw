#include <event_manager.h>

/** @brief Empty event published by the cellular controller to acknowledge
 * reception of proto_out messages from the messaging module. */

struct cellular_ack_event {
    struct event_header header;
};

EVENT_TYPE_DECLARE(cellular_ack_event);


/** @brief Empty event published by the messaging module to acknowledge
 * reception of proto_in messages from the cellular controller. */

struct messaging_ack_event {
    struct event_header header;
};

EVENT_TYPE_DECLARE(messaging_ack_event);



/** @brief outbound proto messages to be sent to the server (binary format).
 * Published by the messaging module and consumed by the cellular controller.
 * */

struct messaging_proto_out_event {
    struct event_header header;
    uint8_t *buf;
    size_t len;
};

EVENT_TYPE_DECLARE(messaging_proto_out_event);


/** @brief inbound proto messages received from the server (binary format).
 * Published by the cellular controller and consumed by the messaging
 * module. */

struct cellular_proto_in_event {
    struct event_header header;
    uint8_t *buf;
    size_t len;
};

EVENT_TYPE_DECLARE(cellular_proto_in_event);

/** @brief Error event published by the cellular controller to when it
 * fails to send a message to the server. */

struct cellular_sending_error_event {
    struct event_header header;
};

EVENT_TYPE_DECLARE(cellular_sending_error_event);