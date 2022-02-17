#include <event_manager.h>
#include "nf_common.h"

/** @brief Last gps fix received from the GNSS driver. */

struct new_gps_fix {
	struct event_header header;
	gps_last_fix_struct_t fix;
};

EVENT_TYPE_DECLARE(new_gps_fix);