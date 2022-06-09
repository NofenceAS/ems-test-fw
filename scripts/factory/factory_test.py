import sys
import os
base_path = os.path.dirname(os.path.abspath(__file__))
lib_path = os.path.join(base_path, "..")
sys.path.append(lib_path)

import nfdiag
import signal
import time
import argparse
import struct

# Parse input arguments
parser = argparse.ArgumentParser(description='Nofence GNSS recorder')
parser.add_argument('--file', help='Base of filename to log to. Name will be postfixed with [time].gnss')
parser.add_argument('--ble', help='Serial number of device in advertised name of device for BLE communication')
parser.add_argument('--rtt', help='Serial number of Segger J-Link to use for RTT communication')
args = parser.parse_args()

# Build connection
if (args.ble and args.rtt):
	raise Exception("Can't connect to both BLE and RTT. Choose one!")
elif (not args.ble) and (not args.rtt):
	print("Didn't specify a connection. RTT will be used on any available J-Link devices.")

stream = None
if args.ble:
	stream = nfdiag.BLEStream("COM4", serial=args.ble)
else:
	stream = nfdiag.JLinkStream(serial=args.rtt)

while not stream.is_connected():
	time.sleep(0.1)

cmndr = nfdiag.Commander(stream)

# Signal handler for stopping with CTRL+C
def signal_handler(sig, frame):
	global cmndr
	global stream

	cmndr.stop()

	del cmndr
	del stream

	sys.exit(0)
signal.signal(signal.SIGINT, signal_handler)

# Sending ping to verify connection
print("Trying to ping...")
start_time = time.time()
got_ping = False
while time.time() < (start_time + 5) and (not got_ping):
	resp = cmndr.send_cmd(0, 0x55)
	if resp:
		got_ping = True
		print("Got ping")
if not got_ping:
	raise Exception("Did not get ping...")

# Running test
resp = cmndr.send_cmd(0x00, 0x7E, b"")

# Interpret result
SELFTEST_FLASH_POS = 0
SELFTEST_EEPROM_POS = 1
SELFTEST_ACCELEROMETER_POS = 2
SELFTEST_GNSS_POS = 3
selftests = [(SELFTEST_FLASH_POS, "Flash"), 
			 (SELFTEST_EEPROM_POS, "EEPROM"), 
			 (SELFTEST_ACCELEROMETER_POS, "Accelerometer"), 
			 (SELFTEST_GNSS_POS, "GNSS")]

failure = False
if resp:
	test_done, test_passed = struct.unpack("<II", resp["data"])

	print("Tests done = " + str(test_done))
	print("Tests passed = " + str(test_passed))

	for selftest in selftests:
		if test_done&(1<<selftest[0]) == 0:
			failure = True
			print("Did not run test: " + selftest[1])
		if test_passed&(1<<selftest[0]) == 0:
			failure = True
			print("Failed test: " + selftest[1])
	if failure:
		raise Exception("!!!!TEST FAILED!!!!!")
	else:
		print("All tests passed!")
else:
	raise Exception("No response when issuing test command")

signal_handler(None, None)