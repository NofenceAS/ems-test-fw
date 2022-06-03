import os
import sys
import time
import logging

# Add scripts folder to search path
base_path = os.path.dirname(os.path.abspath(__file__))
lib_path = os.path.join(base_path, "../../../scripts")
sys.path.append(lib_path)

import nfdiag

def flash(ocd, firmware_image):
    ocd.reset(halt=True)
    ocd.flash_write(firmware_image)
    ocd.flash_verify(firmware_image)
    ocd.resume()

def diagnostics_connect(ocd):
    # Allow application to initialize diagnostics
    time.sleep(2)

    # Start RTT and list channels
    ocd.rtt_prepare()
    # Connect to diagnostics channel
    diag_stream = ocd.rtt_connect("DIAG_UP", 9090)
    
    return nfdiag.Commander(diag_stream)

def trigger_ep(diag_cmndr):
    # EP can not be released before k_uptime_get() returns > MINIMUM_TIME_BETWEEN_BURST
    # Delay for 5 sec
    time.sleep(5.0)

    # Send max sound event to allow EP
    resp = diag_cmndr.send_cmd(nfdiag.GRP_STIMULATOR, nfdiag.CMD_BUZZER_WARN)
    if (not resp) or (resp["code"] != nfdiag.RESP_ACK):
        raise Exception("Failed sending CMD_MAX_SND")
    
    # Release EP
    resp = diag_cmndr.send_cmd(nfdiag.GRP_STIMULATOR, nfdiag.CMD_ELECTRICAL_PULSE):
    if (not resp) or (resp["code"] != nfdiag.RESP_ACK):
        raise Exception("Failed sending CMD_TRG_EP")

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

    diag_stream = None

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
        diag_cmndr = diagnostics_connect(ocd)
        report.end_testcase(testcase)
    except Exception as e:
        report.end_testcase(testcase, fail_message="Test raised exception: " + str(e))
        logging.error("Test raised exception: " + str(e))
        return False

    """ Removed until real hardware is used
    testcase = report.start_testcase(testsuite, "BLE advertisement")
    try:
        ble_scan()
        report.end_testcase(testcase)
    except Exception as e:
        report.end_testcase(testcase, fail_message="Test raised exception: " + str(e))
        logging.error("Test raised exception: " + str(e))
        return False
    """
    
    testcase = report.start_testcase(testsuite, "Trigger EP")
    try:
        trigger_ep(diag_cmndr)
        report.end_testcase(testcase)
    except Exception as e:
        report.end_testcase(testcase, fail_message="Test raised exception: " + str(e))
        logging.error("Test raised exception: " + str(e))
        return False

    return True
