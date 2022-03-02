from __future__ import print_function

from time import gmtime, strftime, sleep
from bluepy.btle import Scanner, DefaultDelegate, BTLEException
import sys
import time

"""
class ScanDelegate(DefaultDelegate):
    def handleDiscovery(self, dev, isNewDev, isNewData):
        print(strftime("%Y-%m-%d %H:%M:%S", gmtime()), dev.addr, dev.getScanData())
        sys.stdout.flush()
"""


class BlueFence:
    def __init__(self):
        pass
    
    def scan(self):
        scanner = Scanner() #.withDelegate(ScanDelegate())

        scanner.start(passive=True)

        start = time.time()
        while (time.time()-start) < 10.0:
            scanner.process(0.25)
            print(scanner.getDevices())

        scanner.stop()