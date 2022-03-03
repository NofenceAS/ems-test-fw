import os
import logging

def flash(ocd, firmware_image):
    ocd.reset(halt=True)
    ocd.flash_write(firmware_image)
    ocd.flash_verify(firmware_image)
    ocd.resume()

def diagnostics_connect(ocd):
    ocd.rtt_prepare()
    # TODO - Check for channels
    # TODO - Connect to diagnostics channel

import bluefence

def ble_scan():
    board_ble_address = os.environ["X25_BOARD_BLE_ADDRESS"]
    device = bluefence.scan_for_device(addr=board_ble_address)

    if device == None:
        raise Exception("Did not find device...")
    
    logging.info("Found device " + str(device))

    ble = bluefence.BlueFence(device)
    company_id = ble.get_adv_data(bluefence.BlueFence.BLE_MFG_IDX_COMPANY_ID)
    battery_level = ble.get_adv_data(bluefence.BlueFence.BLE_MFG_IDX_BATTERY)

    logging.info("company_id=" + "0x{:04x}".format(company_id))
    logging.info("battery_level=" + str(battery_level))

    NOFENCE_BLUETOOTH_SIG_COMPANY_ID = 0x05AB
    if company_id != NOFENCE_BLUETOOTH_SIG_COMPANY_ID:
        raise Exception("Unexpected company ID in advertisement data!")
    
    if (0 > battery_level) and (battery_level > 100):
        raise Exception("Unexpected battery level in advertisement data!")

def run(dep):
    report = dep["report"]
    testsuite = report.create_testsuite("x25_basic_test")

    ocd = dep["ocd"]
    build_path = dep["buildpath"]

    zephyr_path = os.path.join(build_path, "zephyr")
    combined_image = os.path.join(zephyr_path, "merged.hex")

    testcase = report.start_testcase(testsuite, "Flash image")
    try:
        flash(ocd, combined_image)
        report.end_testcase(testcase)
    except Exception as e:
        report.end_testcase(testcase, fail_message="Test raised exception: " + str(e))
        logging.error("Test raised exception: " + str(e))
        return False
        
    testcase = report.start_testcase(testsuite, "Diagnostics connect")
    try:
        diagnostics_connect(ocd)
        report.end_testcase(testcase)
    except Exception as e:
        report.end_testcase(testcase, fail_message="Test raised exception: " + str(e))
        logging.error("Test raised exception: " + str(e))
        return False

    testcase = report.start_testcase(testsuite, "BLE advertisement")
    try:
        ble_scan()
        report.end_testcase(testcase)
    except Exception as e:
        report.end_testcase(testcase, fail_message="Test raised exception: " + str(e))
        logging.error("Test raised exception: " + str(e))
        return False

    return True
