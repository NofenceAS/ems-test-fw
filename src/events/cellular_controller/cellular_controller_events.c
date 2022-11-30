#include "cellular_controller_events.h"

EVENT_TYPE_DEFINE(cellular_ack_event,      /* Unique event name. */
                  false,              /* Event logged by default. */
                  NULL,  /* Callback Function. */
                  NULL);             /* No event info provided. */



EVENT_TYPE_DEFINE(cellular_proto_in_event,
                  false,
                  NULL,
                  NULL);

EVENT_TYPE_DEFINE(cellular_error_event,
                  false,
                  NULL,
                  NULL);

EVENT_TYPE_DEFINE(connection_state_event,
		  false,
		  NULL,
		  NULL);

EVENT_TYPE_DEFINE(modem_state,
		  false,
		  NULL,
		  NULL);

EVENT_TYPE_DEFINE(free_message_mem_event,
		  false,
		  NULL,
		  NULL);

EVENT_TYPE_DEFINE(gsm_info_event,
		  false,
		  NULL,
		  NULL);

EVENT_TYPE_DEFINE(request_gsm_info_event,
		  false,
		  NULL,
		  NULL);