
#ifndef X3_FW_AMC_EVENTS_H
#define X3_FW_AMC_EVENTS_H
#include "amc_zone.h"
#include <event_manager.h>

struct zone_change {
	struct event_header header;
	amc_zone_t zone;
};

EVENT_TYPE_DECLARE(zone_change);

struct xy_location {/*TODO: this event needs to be published by amc after gnss_calc_xy()*/
	struct event_header header;
	int16_t x;
	int16_t y;
};

EVENT_TYPE_DECLARE(xy_location);


#endif //X3_FW_AMC_EVENTS_H
