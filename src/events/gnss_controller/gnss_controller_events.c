#include "gnss_controller_events.h"

EVENT_TYPE_DEFINE(new_gnss_fix,
		  true,
		  NULL,
		  NULL);

EVENT_TYPE_DEFINE(new_gnss_data,
		  true,
		  NULL,
		  NULL);

EVENT_TYPE_DEFINE(gnss_rate,
		  true,
		  NULL,
		  NULL);

EVENT_TYPE_DEFINE(gnss_switch_on,
		  true,
		  NULL,
		  NULL);

EVENT_TYPE_DEFINE(gnss_switch_off,
		  true,
		  NULL,
		  NULL);

EVENT_TYPE_DEFINE(gnss_set_mode,
		  true,
		  NULL,
		  NULL);