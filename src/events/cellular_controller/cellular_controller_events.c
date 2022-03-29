#include "cellular_controller_events.h"

EVENT_TYPE_DEFINE(cellular_ack_event,      /* Unique event name. */
                  true,              /* Event logged by default. */
                  NULL,  /* Callback Function. */
                  NULL);             /* No event info provided. */



EVENT_TYPE_DEFINE(cellular_proto_in_event,
                  true,
                  NULL,
                  NULL);

EVENT_TYPE_DEFINE(cellular_error_event,
                  true,
                  NULL,
                  NULL);

EVENT_TYPE_DEFINE(connection_state_event,
		  true,
		  NULL,
		  NULL);