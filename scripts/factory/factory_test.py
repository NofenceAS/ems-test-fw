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
parser.add_argument('--ble', help='Serial number of device in advertised name of device for BLE communication')
parser.add_argument('--rtt', help='Serial number of Segger J-Link to use for RTT communication')
parser.add_argument('--jlinkpath', help='Path to JLink executable')
parser.add_argument('--suid', help='DUT Unique serial number', required=True)
parser.add_argument('--pt', help='DUT Product type', required=True)
parser.add_argument('--ems', help='DUT EMS provider', required=True)
args = parser.parse_args()

# Build connection
if (args.ble and args.rtt):
	raise Exception("Can't connect to both BLE and RTT. Choose one!")
elif (not args.ble) and (not args.rtt):
	print("Didn't specify a connection. RTT will be used on any available J-Link devices.")

#print(args.suid + " will be flashed into DUT")
#print(args.pt + " will be flashed into DUT")

stream = None
if args.ble:
	stream = nfdiag.BLEStream("COM4", serial=args.ble)
else:
	stream = nfdiag.JLinkStream(serial=args.rtt, jlink_path=args.jlinkpath)

try_until = time.time()+10
while not stream.is_connected():
	time.sleep(0.1)
	if time.time() > try_until:
		raise Exception("Timed out waiting for connection...")

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
	resp = cmndr.send_cmd(nfdiag.GROUP_SYSTEM, nfdiag.CMD_PING)
	if resp:
		got_ping = True
		print("Got ping")
if not got_ping:
	raise Exception("Did not get ping...")

# Write settings 
 # TODO: What values?

if not cmndr.write_setting(nfdiag.ID_SERIAL, int(args.suid)):
	raise Exception("Failed to write settings")
if not cmndr.write_setting(nfdiag.ID_HOST_PORT, b"172.31.36.11:4321\x00"):
	raise Exception("Failed to write settings")
if not cmndr.write_setting(nfdiag.ID_EMS_PROVIDER, int(args.ems)):
	raise Exception("Failed to write settings")
if not cmndr.write_setting(nfdiag.ID_PRODUCT_RECORD_REV, 2):
	raise Exception("Failed to write settings")
if not cmndr.write_setting(nfdiag.ID_BOM_MEC_REV, 5):
	raise Exception("Failed to write settings")
if not cmndr.write_setting(nfdiag.ID_BOM_PCB_REV, 2):
	raise Exception("Failed to write settings")
if not cmndr.write_setting(nfdiag.ID_HW_VERSION, 20):
	raise Exception("Failed to write settings")
if not cmndr.write_setting(nfdiag.ID_PRODUCT_TYPE, int(args.pt)):
	raise Exception("Failed to write settings")

print("DUT Serial No: " + str(cmndr.read_setting(nfdiag.ID_SERIAL)))
print("HOST PORT: " + str(cmndr.read_setting(nfdiag.ID_HOST_PORT)))
print("EMS Provider: " + str(cmndr.read_setting(nfdiag.ID_EMS_PROVIDER)))
print("Product type: " + str(cmndr.read_setting(nfdiag.ID_PRODUCT_TYPE)))

# Running test
resp = cmndr.send_cmd(nfdiag.GROUP_SYSTEM, nfdiag.CMD_TEST, b"")

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

	print("Tests done = " + "{0:b}".format(test_done))
	print("Tests passed = " + "{0:b}".format(test_passed))

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
	raise Exception("No response when issuing test command")

print("Giving a pulse in 5 seconds!!!!!!!!!!")
time.sleep(5)
val = cmndr.electric_pulse_now()

# Read CCID from modem
ccid = b""
timeout = time.time()+15
while len(ccid) == 0:
	ccid = cmndr.modem_get_ccid()
	if ccid is None:
		raise Exception("No response while reading CCID")
	if time.time() > timeout:
		raise Exception("Timed out waiting for valid CCID. Modem failure!")
print("Modem CCID: " + str(int(ccid)))

# Read settings
set_file = open(args.suid + "_" + str(int(time.time())) + ".log", "w")

val = cmndr.read_setting(nfdiag.ID_SERIAL)
if val is None:
	raise Exception("Failed to read settings")
set_file.write("ID_SERIAL = " + str(val) + "\n")

val = cmndr.read_setting(nfdiag.ID_HOST_PORT)
if val is None:
	raise Exception("Failed to read settings")
set_file.write("ID_HOST_PORT = " + str(val) + "\n")

val = cmndr.read_setting(nfdiag.ID_EMS_PROVIDER)
if val is None:
	raise Exception("Failed to read settings")
set_file.write("ID_EMS_PROVIDER = " + str(val) + "\n")

val = cmndr.read_setting(nfdiag.ID_PRODUCT_RECORD_REV)
if val is None:
	raise Exception("Failed to read settings")
set_file.write("ID_PRODUCT_RECORD_REV = " + str(val) + "\n")

val = cmndr.read_setting(nfdiag.ID_BOM_MEC_REV)
if val is None:
	raise Exception("Failed to read settings")
set_file.write("ID_BOM_MEC_REV = " + str(val) + "\n")

val = cmndr.read_setting(nfdiag.ID_BOM_PCB_REV)
if val is None:
	raise Exception("Failed to read settings")
set_file.write("ID_BOM_PCB_REV = " + str(val) + "\n")

val = cmndr.read_setting(nfdiag.ID_HW_VERSION)
if val is None:
	raise Exception("Failed to read settings")
set_file.write("ID_HW_VERSION = " + str(val) + "\n")

val = cmndr.read_setting(nfdiag.ID_PRODUCT_TYPE)
if val is None:
	raise Exception("Failed to read settings")
set_file.write("ID_PRODUCT_TYPE = " + str(val) + "\n")

set_file.write("CCID = " + str(int(ccid)) + "\n")

set_file.close()

print("Success!")

# Testing done, tear down
signal_handler(None, None)