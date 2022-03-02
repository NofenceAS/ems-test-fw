import os

def flash(ocd, firmware_image):
    ocd.reset(halt=True):
    ocd.flash_write(firmware_image)
    ocd.flash_verify(firmware_image)
    ocd.resume()

def ble_scan(ble):
    board_ble_address = os.environ["X25_BOARD_BLE_ADDRESS"]
    device = ble.scan_for_device(addr=board_ble_address)

    if device == None:
        raise Exception("Did not find device...")
    
    print("Found device " + str(device))
    # TODO - Verify ScanData
    print("    ScanData " + str(device.getScanData()))

def run(dep):
    ocd = dep["ocd"]
    ble = dep["ble"]
    build_path = dep["buildpath"]

    zephyr_path = os.path.join(build_path, "zephyr")
    combined_image = os.path.join(zephyr_path, "merged.hex")

    try:
        flash(ocd, combined_image)
    except Exception as e:
        return False

    try:
        ble_scan(ble)
    except Exception as e:
        return False

    return True
