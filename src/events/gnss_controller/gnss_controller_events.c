#include "gnss_controller_events.h"

EVENT_TYPE_DEFINE(gnss_data, false, NULL, NULL);

EVENT_TYPE_DEFINE(gnss_set_mode_event, false, NULL, NULL);

EVENT_TYPE_DEFINE(gnss_mode_changed_event, false, NULL, NULL);

#if defined(CONFIG_DIAGNOSTIC_EMS_FW)
EVENT_TYPE_DEFINE(gnss_fwhw_info_event, false, NULL, NULL);
#endif
