def run(ble):
    device = ble.scan_for_device(addr="E9:F5:E1:40:45:A8")

    if device == None:
        raise Exception("Did not find device...")
    
    print("Found device " + str(device))
    print("    ScanData " + str(device.getScanData()))