from __future__ import print_function

from time import gmtime, strftime, sleep
from bluepy.btle import Scanner, DefaultDelegate, BTLEException
import sys
import time

def scan_for_device(self, addr=None, uuid=None):
    result_device = None
    
    scanner = Scanner()

    scanner.start(passive=True)

    start = time.time()
    while (not result_device) and ((time.time()-start) < 10.0):
        scanner.process(0.25)
        devices = scanner.getDevices()
        for dev in devices:
            match = True
            if addr:
                if dev.addr.lower() != addr.lower():
                    match = False
            if uuid:
                # TODO - Check for UUID
                pass
            if match:
                result_device = dev

    scanner.stop()

    return result_device

class BlueFence:
    BLE_MFG_IDX_COMPANY_ID = (0, 2)
    BLE_MFG_IDX_NRF_FW_VER = (2, 2)
    BLE_MFG_IDX_SERIAL_NR = (4, 4)
    BLE_MFG_IDX_BATTERY = (8, 1)
    BLE_MFG_IDX_ERROR = (9, 1)
    BLE_MFG_IDX_COLLAR_MODE = (10, 1)
    BLE_MFG_IDX_COLLAR_STATUS = (11, 1)
    BLE_MFG_IDX_FENCE_STATUS = (12, 1)
    BLE_MFG_IDX_VALID_PASTURE = (13, 1)
    BLE_MFG_IDX_FENCE_DEF_VER = (14, 2)
    BLE_MFG_IDX_HW_VER = (16, 1)
    BLE_MFG_IDX_ATMEGA_VER = (17, 2)

    def __init__(self, device):
        self._device = device

        scandata = self._device.getScanData()
        logging.info("    ScanData " + str(scandata))

        # Verify ScanData
        for data in scandata:
            if data[1] == "Manufacturer":
                self._mfg_data = bytes.fromhex(data[2])
    
    def get_adv_data(self, param):
        index = param[0]
        size = param[1]

        data = 0
        for i in range(size):
            data = data << 8
            data += self._mfg_data[index+(size-1-i)]
        
        return data