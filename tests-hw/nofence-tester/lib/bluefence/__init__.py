from __future__ import print_function

from time import gmtime, strftime, sleep
from bluepy.btle import Scanner, DefaultDelegate, BTLEException
import sys
import time

class BlueFence:
    def __init__(self):
        pass
    
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