#include "messaging_module_events.h"

EVENT_TYPE_DEFINE(messaging_ack_event,
                  true,
                  NULL,
                  NULL);

EVENT_TYPE_DEFINE(messaging_proto_out_event,
                  true,
                  NULL,
                  NULL);

EVENT_TYPE_DEFINE(messaging_stop_connection_event,
                  true,
                  NULL,
                  NULL);
