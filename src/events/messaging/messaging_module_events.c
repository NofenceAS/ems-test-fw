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

EVENT_TYPE_DEFINE(messaging_host_address_event,
		  true,
		  NULL,
		  NULL);

/* State update events (published by AMC) */
EVENT_TYPE_DEFINE(update_collar_mode,
		  true,
		  NULL,
		  NULL);

EVENT_TYPE_DEFINE(update_collar_status,
		  true,
		  NULL,
		  NULL);

EVENT_TYPE_DEFINE(update_fence_status,
		  true,
		  NULL,
		  NULL);

EVENT_TYPE_DEFINE(update_fence_version,
		  true,
		  NULL,
		  NULL);

EVENT_TYPE_DEFINE(update_flash_erase,
		  true,
		  NULL,
		  NULL);

EVENT_TYPE_DEFINE(update_zap_count,
		  true,
		  NULL,
		  NULL);

EVENT_TYPE_DEFINE(new_fence_available,
		  true,
		  NULL,
		  NULL);

EVENT_TYPE_DEFINE(request_ano_event,
		  true,
		  NULL,
		  NULL);

EVENT_TYPE_DEFINE(ano_ready,
		  true,
		  NULL,
		  NULL);

EVENT_TYPE_DEFINE(animal_warning_event,
		  true,
		  NULL,
		  NULL);

EVENT_TYPE_DEFINE(animal_escape_event,
		  true,
		  NULL,
		  NULL);