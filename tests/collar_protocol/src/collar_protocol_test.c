#include <ztest.h>
#include "collar_protocol.h"

static void test_encode_valid_data(void)
{
	NofenceMessage reqMsg = {
		.header = {
			.has_ulVersion = true,
			.ulVersion = 0,
			.ulId = 123,
		},
		.which_m = NofenceMessage_firmware_upgrade_req_tag,
		.m.firmware_upgrade_req = {
			.ulVersion = 999998,  // Set later
			.has_ucHardwareVersion = true,
			.ucHardwareVersion = 13,
			.has_versionInfoHW = true,
			.versionInfoHW.ucPCB_RF_Version = 13,
			.versionInfoHW.usPCB_Product_Type = 1,
			.versionInfoHW.ucPCB_HV_Version = 0, 
			.usFrameNumber = 0
		}
	};
	uint8_t raw_buf[123];
	size_t raw_size = 0;
	zassert_equal(0, collar_protocol_encode(&reqMsg, raw_buf, sizeof(raw_buf), &raw_size), "collar_protocol_encode returned error");
}

void test_main(void)
{
        ztest_test_suite(common,
                         ztest_unit_test(test_encode_valid_data)
                         );
        ztest_run_test_suite(common);
}