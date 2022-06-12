#ifndef X3_FW_HISTOGRAM_EVENTS_H
#define X3_FW_HISTOGRAM_EVENTS_H

#include "collar_protocol.h"
#include "event_manager.h"

typedef struct collar_histogram{
	_HISTOGRAM_ANIMAL_BEHAVE animal_behave;
	_HISTOGRAM_ZONE in_zone;
	_POSITION_QC_MAX_MIN_MEAN qc_baro_gps_max_mean_min;
	_HISTOGRAM_CURRENT_PROFILE current_profile;
	_BATTERY_QC qc_battery;
} collar_histogram;

extern struct k_msgq histogram_msgq;

struct save_histogram {
	struct event_header header;
};
EVENT_TYPE_DECLARE(save_histogram);

#endif //X3_FW_HISTOGRAM_EVENTS_H
