//
// Created by per on 27.08.2021.
//

#ifndef X3_FW_NF_BLE_H
#define X3_FW_NF_BLE_H

#include <bluetooth/uuid.h>


#define BEACON_DATA_LEN   21

#define  BEACON_MAJOR_ID  6631
#define  BEACON_MINOR_ID  6415
#define  NOFENCE_BLUETOOTH_SIG_COMPANY_ID  1451
#define  NOFENCE_BLUETOOTH_COLLAR_RSSI_DBM_1_M  -60


void nf_ble_init();
void nf_ble_start_scan();

#endif //X3_FW_NF_BLE_H
