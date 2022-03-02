import os

def flash(ocd, firmware_image):
    ocd.reset(halt=True)
    ocd.flash_write(firmware_image)
    ocd.flash_verify(firmware_image)
    ocd.resume()


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
    ocd = dep["ocd"]
    build_path = dep["buildpath"]

    zephyr_path = os.path.join(build_path, "zephyr")
    combined_image = os.path.join(zephyr_path, "merged.hex")

    try:
        flash(ocd, combined_image)
    except Exception as e:
        logging.error("Test raised exception: " + str(e))
        return False

    try:
        ble_scan()
    except Exception as e:
        logging.error("Test raised exception: " + str(e))
        return False

    return True
