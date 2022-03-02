def run(ble):
    device = ble.scan_for_device(addr="E9:F5:E1:40:45:A8")

    print("Found device " + str(device))