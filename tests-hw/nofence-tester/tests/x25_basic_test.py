import os

def flash(ocd, firmware_image):
    ocd.reset(halt=True)
    ocd.flash_write(firmware_image)
    ocd.resume()

    return True

def ble_scan(ble):
    board_ble_address = os.environ["X25_BOARD_BLE_ADDRESS"]
    device = ble.scan_for_device(addr=board_ble_address)

    if device == None:
        raise Exception("Did not find device...")
    
    print("Found device " + str(device))
    # TODO - Verify ScanData
    print("    ScanData " + str(device.getScanData()))
    
    return True

def run(dep):
    ocd = dep["ocd"]
    ble = dep["ble"]
    build_path = dep["buildpath"]

    zephyr_path = os.path.join(build_path, "zephyr")
    combined_image = os.path.join(zephyr_path, "merged.hex")

    if not flash(ocd, combined_image):
        return False

    if not ble_scan(ble):
        return False

    return True
