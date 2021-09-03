
#include <unity.h>
#include <beacon_processor.h>
#include <stdbool.h>
#include <nf_ble.h>


#define MAC1 {0x00, 0x01,0x02,0x03,0x04,0x05}
#define MAC2 {0x01, 0x01,0x02,0x03,0x04,0x05}
#define MAC3 {0x02, 0x01,0x02,0x03,0x04,0x05}
#define MAC4 {0x03, 0x01,0x02,0x03,0x04,0x05}
#define MAC5 {0x04, 0x01,0x02,0x03,0x04,0x05}
#define MAC6 {0x05, 0x01,0x02,0x03,0x04,0x05}

#define DEFAULT_EVT(MAC)  \
{\
        .evt_type = BLE_SCAN_BEACON_ADVERTISER_FOUND, \
        .rssi_sample_value = 56, \
        .mac_address.addr = MAC, \
        .adv_data = { \
                    .beacon_dev_type = 1, \
                    .major = Constants_BEACON_MAJOR_ID, \
                    .minor = Constants_BEACON_MINOR_ID, \
                    .rssi = 205,  \
                    .uuid = {0x01, 0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F} \
        } \
}

void setUp(void) {
}

/* Suite teardown shall finalize with mandatory call to generic_suiteTearDown. */
extern int generic_suiteTearDown(int num_failures);

int test_suiteTearDown(int num_failures) {
    return generic_suiteTearDown(num_failures);
}

void test_beacon_init(void) {
    beac_init();
}

#define UUID_1  BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0x6E400001, 0xB5A3, 0xF393, 0xE0A9, 0xE50E24DCCA9E))


void test_beacon_process_event() {
    beac_init();
    adv_data_t adv_data = {
            .major = BEACON_MAJOR_ID,
            .minor = BEACON_MINOR_ID,
            .rssi = 210,
            .uuid = {*(UUID_1)}
    };
    bt_addr_le_t addr1 = BT_ADDR_LE_ANY[0];
    addr1.a.val[0] = 0xDE;

    beac_process_event(0, &addr1, 56, &adv_data);
    SingleBeaconInfo_t *pInfo = beac_get_nearest_beacon(0);
    TEST_ASSERT_EQUAL(4, pInfo->calculated_dist);
}


void main(void) {
    extern void unity_main();
    (void) unity_main();
}
