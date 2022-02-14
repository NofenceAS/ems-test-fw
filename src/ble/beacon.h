//
// Created by per on 30.08.2021.
//

#ifndef X3_FW_BEACON_H
#define X3_FW_BEACON_H

#include <bluetooth/uuid.h>

/** @brief decoded advertising data*/
typedef struct {
	uint16_t manuf_id; /** manufacturer specific identifier*/
	uint8_t beacon_dev_type; /** manufacturer data type*/
	struct bt_uuid_128 uuid; /** 128 uuid*/
	uint16_t major; /** beacon Major*/
	uint16_t minor; /** beacon Minor*/
	uint8_t rssi; /** beacon Rssi */
} adv_data_t;

typedef struct {
	uint16_t manuf_id; /** manufacturer specific identifier*/
	uint8_t beacon_dev_type; /** manufacturer data type*/
	struct bt_uuid_128 uuid; /** 128 uuid*/
	uint16_t major; /** beacon Major*/
	uint16_t minor; /** beacon Minor*/
	uint8_t rssi; /** beacon Rssi */
} adv_service_data_t;

#endif //X3_FW_BEACON_H
