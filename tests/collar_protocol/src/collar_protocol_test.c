#include <ztest.h>
#include "collar_protocol.h"

static void test_encode_valid_data(void) 
{
	int result;
	NofenceMessage message;
	NofenceMessage reqMsg;
	uint8_t raw_buf[NofenceMessage_size];
	size_t raw_size = 0;

	memset(&message, 0, sizeof(message));
	memset(&reqMsg, 0, sizeof(reqMsg));

	reqMsg.header.has_ulVersion = true;
	reqMsg.header.ulVersion = 0;
	reqMsg.header.ulId = 123;
	reqMsg.header.ulUnixTimestamp = 1234;
	reqMsg.which_m = NofenceMessage_firmware_upgrade_req_tag;
	reqMsg.m.firmware_upgrade_req.ulVersion = 999998;
	reqMsg.m.firmware_upgrade_req.has_ucHardwareVersion = true;
	reqMsg.m.firmware_upgrade_req.ucHardwareVersion = 13;
	reqMsg.m.firmware_upgrade_req.has_versionInfoHW = true;
	reqMsg.m.firmware_upgrade_req.versionInfoHW.ucPCB_RF_Version = 13;
	reqMsg.m.firmware_upgrade_req.versionInfoHW.usPCB_Product_Type = 1;
	reqMsg.m.firmware_upgrade_req.versionInfoHW.ucPCB_HV_Version = 0;
	reqMsg.m.firmware_upgrade_req.usFrameNumber = 0;
	
	result = collar_protocol_encode(&reqMsg, raw_buf, sizeof(raw_buf), \
					&raw_size);
	
	zassert_equal(result, 0, \
			"collar_protocol_encode failed for valid data");

	result = collar_protocol_decode(raw_buf, raw_size, &message);
	zassert_equal(result, 0, \
			"collar_protocol_decode failed for valid data");

	result = memcmp(&reqMsg, &message, sizeof(NofenceMessage));
	zassert_equal(result, 0, \
			"collar_protocol_decode resulted in unexpected data");
}

static void test_decode_valid_data(void)
{
	int result;
	NofenceMessage message;
	uint8_t raw_buf[] = {0x0A, 0x0C, 0x08, 0xB9, 0x0A, 0x10, 0xD4, 0xCB, \
		0xC4, 0x8D, 0x06, 0x18, 0xC7, 0x02, 0x22, 0x10, 0x08, 0xBE, \
		0xDE, 0x38, 0x10, 0x10, 0x18, 0x0E, 0x22, 0x06, 0x08, 0x0C, \
		0x10, 0x00, 0x18, 0x01};
	result = collar_protocol_decode(raw_buf, sizeof(raw_buf), &message);
	zassert_equal(result, 0, \
			"collar_protocol_decode failed for valid data");

	zassert_true(message.header.has_ulVersion, \
			"collar_protocol_decode did not decode ulVersion");
	zassert_equal(message.header.ulVersion, 327, \
			"collar_protocol_decode return wrong ulVersion");
	zassert_equal(message.header.ulId, 1337, \
			"collar_protocol_decode return wrong ulId");
	zassert_equal(message.header.ulUnixTimestamp, 1638999508, \
			"collar_protocol_decode return wrong ulUnixTimestamp");
	zassert_equal(message.which_m, NofenceMessage_firmware_upgrade_req_tag, \
			"collar_protocol_decode return wrong which_m");
	zassert_equal(message.m.firmware_upgrade_req.ulVersion, 929598, \
			"collar_protocol_decode return wrong ulVersion");
	zassert_true(message.m.firmware_upgrade_req.has_ucHardwareVersion, \
			"collar_protocol_decode did not decode ucHardwareVersion");
	zassert_equal(message.m.firmware_upgrade_req.ucHardwareVersion, 14, \
			"collar_protocol_decode return wrong ucHardwareVersion");
	zassert_true(message.m.firmware_upgrade_req.has_versionInfoHW, \
			"collar_protocol_decode did not decode versionInfoHW");
	zassert_equal(message.m.firmware_upgrade_req.versionInfoHW.ucPCB_RF_Version, 12, \
			"collar_protocol_decode return wrong ucPCB_RF_Version");
	zassert_equal(message.m.firmware_upgrade_req.versionInfoHW.usPCB_Product_Type, 1, \
			"collar_protocol_decode return wrong usPCB_Product_Type");
	zassert_equal(message.m.firmware_upgrade_req.versionInfoHW.ucPCB_HV_Version, 0, \
			"collar_protocol_decode return wrong ucPCB_HV_Version");
	zassert_equal(message.m.firmware_upgrade_req.usFrameNumber, 16, \
			"collar_protocol_decode return wrong usFrameNumber");
}

static void test_invalid_input(void) {
	/* Valid protobuf data of firmware_upgrade_req */
	int result;
	size_t raw_size = 0;
	uint8_t raw_buf[] = {0x0A, 0x0C, 0x08, 0xB9, 0x0A, 0x10, 0xD4, 0xCB, \
			0xC4, 0x8D, 0x06, 0x18, 0xC7, 0x02, 0x22, 0x10, 0x08, \
			0xBE, 0xDE, 0x38, 0x10, 0x10, 0x18, 0x0E, 0x22, 0x06, \
			0x08, 0x0C, 0x10, 0x00, 0x18, 0x01};
	NofenceMessage message;
	const char* decode_invalid_msg = "collar_protocol_decode returned \
					  wrong error for invalid arguments";
	const char* encode_invalid_msg = "collar_protocol_encode returned \
					  wrong error for invalid arguments";
	
	/* Test decode with invalid input arguments */
	result = collar_protocol_decode(raw_buf, sizeof(raw_buf), NULL);
	zassert_equal(result, -EINVAL, decode_invalid_msg);
	result = collar_protocol_decode(raw_buf, 0, &message);
	zassert_equal(result, -EINVAL, decode_invalid_msg);
	result = collar_protocol_decode(raw_buf, 0, NULL);
	zassert_equal(result, -EINVAL, decode_invalid_msg);
	result = collar_protocol_decode(NULL, sizeof(raw_buf), &message);
	zassert_equal(result, -EINVAL, decode_invalid_msg);
	result = collar_protocol_decode(NULL, sizeof(raw_buf), NULL);
	zassert_equal(result, -EINVAL, decode_invalid_msg);
	result = collar_protocol_decode(NULL, 0, &message);
	zassert_equal(result, -EINVAL, decode_invalid_msg);
	result = collar_protocol_decode(NULL, 0, NULL);
	zassert_equal(result, -EINVAL, decode_invalid_msg);

	/* Decode raw_buf, should work since it contains valid data and was tested previously */
	result = collar_protocol_decode(raw_buf, sizeof(raw_buf), &message);
	zassert_equal(result, 0, \
			"collar_protocol_decode returned unexpected error");

	/* Test encode with invalid input arguments */
	result = collar_protocol_encode(&message, raw_buf, sizeof(raw_buf), NULL);
	zassert_equal(result, -EINVAL, encode_invalid_msg);
	
	result = collar_protocol_encode(&message, raw_buf, 0, &raw_size);
	zassert_equal(result, -EINVAL, encode_invalid_msg);

	result = collar_protocol_encode(&message, raw_buf, 0, NULL);
	zassert_equal(result, -EINVAL, encode_invalid_msg);

	result = collar_protocol_encode(&message, NULL, sizeof(raw_buf), &raw_size);
	zassert_equal(result, -EINVAL, encode_invalid_msg);

	result = collar_protocol_encode(&message, NULL, sizeof(raw_buf), NULL);
	zassert_equal(result, -EINVAL, encode_invalid_msg);

	result = collar_protocol_encode(&message, NULL, 0, &raw_size);
	zassert_equal(result, -EINVAL, encode_invalid_msg);

	result = collar_protocol_encode(&message, NULL, 0, NULL);
	zassert_equal(result, -EINVAL, encode_invalid_msg);

	result = collar_protocol_encode(NULL, raw_buf, sizeof(raw_buf), &raw_size);
	zassert_equal(result, -EINVAL, encode_invalid_msg);

	result = collar_protocol_encode(NULL, raw_buf, sizeof(raw_buf), NULL);
	zassert_equal(result, -EINVAL, encode_invalid_msg);

	result = collar_protocol_encode(NULL, raw_buf, 0, &raw_size);
	zassert_equal(result, -EINVAL, encode_invalid_msg);

	result = collar_protocol_encode(NULL, raw_buf, 0, NULL);
	zassert_equal(result, -EINVAL, encode_invalid_msg);

	result = collar_protocol_encode(NULL, NULL, sizeof(raw_buf), &raw_size);
	zassert_equal(result, -EINVAL, encode_invalid_msg);

	result = collar_protocol_encode(NULL, NULL, sizeof(raw_buf), NULL);
	zassert_equal(result, -EINVAL, encode_invalid_msg);

	result = collar_protocol_encode(NULL, NULL, 0, &raw_size);
	zassert_equal(result, -EINVAL, encode_invalid_msg);

	result = collar_protocol_encode(NULL, NULL, 0, NULL);
	zassert_equal(result, -EINVAL, encode_invalid_msg);
}

static void test_buffer_overflow(void) {
	int result;
	NofenceMessage message;
	size_t raw_size = 0;

	/* Valid protobuf data of firmware_upgrade_req */
	uint8_t raw_buf[] = {0x0A, 0x0C, 0x08, 0xB9, 0x0A, 0x10, 0xD4, 0xCB, \
		0xC4, 0x8D, 0x06, 0x18, 0xC7, 0x02, 0x22, 0x10, 0x08, 0xBE, \
		0xDE, 0x38, 0x10, 0x10, 0x18, 0x0E, 0x22, 0x06, 0x08, 0x0C, \
		0x10, 0x00, 0x18, 0x01};

	/* Decode raw_buf, should work since it contains valid data and was 
	tested previously */
	result = collar_protocol_decode(raw_buf, sizeof(raw_buf), &message);
	zassert_equal(result, 0, "collar_protocol_decode failed for valid \
				  data");

	/* Attempt encode with buffer size one less than required */
	result = collar_protocol_encode(&message, raw_buf, sizeof(raw_buf)-1, \
					&raw_size);
	zassert_equal(result, -EMSGSIZE, "collar_protocol_encode returned \
			wrong result for insufficient buffer size (-1)");

	/* Attempt encode with buffer size half of required */
	result = collar_protocol_encode(&message, raw_buf, sizeof(raw_buf)/2, \
					&raw_size);
	zassert_equal(result, -EMSGSIZE, "collar_protocol_encode returned \
			wrong result for insufficient buffer size (/2)");

	/* Attempt encode with buffer size of 1 */
	result = collar_protocol_encode(&message, raw_buf, 1, &raw_size);
	zassert_equal(result, -EMSGSIZE, "collar_protocol_encode returned \
			wrong result for insufficient buffer size (=1)");
}

static void test_corrupted_decode(void) {
	int result;
	NofenceMessage message;
	/* Attempt decode with corrupted buffer data */
	uint8_t corrupted_raw_buf[] = {0xFF, 0xFE, 0x99, 0x78, 0x0A, 0x10, \
		0xD4, 0xFF, 0xC4, 0x8D, 0x00, 0x18, 0xC7, 0x02, 0xAA, 0x10, \
		0x08, 0xBE, 0xDE, 0x38, 0x10, 0x10, 0x18, 0x0E, 0x22, 0x06, \
		0x08, 0x0C, 0x10, 0x00, 0x18, 0x01};
	
	result = collar_protocol_decode(corrupted_raw_buf, \
					sizeof(corrupted_raw_buf), &message);
	
	zassert_equal(result, -EILSEQ, "collar_protocol_decode returned wrong \
					result code for corrupted data input");
}

void test_main(void)
{
	ztest_test_suite(common,
			ztest_unit_test(test_encode_valid_data),
			ztest_unit_test(test_decode_valid_data),
			ztest_unit_test(test_invalid_input),
			ztest_unit_test(test_buffer_overflow),
			ztest_unit_test(test_corrupted_decode)
			);
	ztest_run_test_suite(common);
}