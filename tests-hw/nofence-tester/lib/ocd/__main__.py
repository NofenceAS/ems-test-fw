import logging
import time
from . import OCD

logging.basicConfig(encoding='utf-8', level=logging.DEBUG)

ocd = OCD()
ocd.reset(halt=True)
ocd.flash_write("/home/pi/zephyr.hex")
ocd.resume()
ocd.rtt_prepare()
ocd.rtt_connect(0, 9090)
time.sleep(10)
del ocd