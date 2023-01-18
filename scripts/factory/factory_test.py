import sys
import re
import subprocess
import os
import time
import random
from threading import Thread

base_path = os.path.dirname(os.path.abspath(__file__))
lib_path = os.path.join(base_path, "..")
sys.path.append(lib_path)

import nfdiag
import signal
import time
import argparse
import struct
from collections import namedtuple

# Parse input arguments
parser = argparse.ArgumentParser(description='Nofence GNSS recorder')
parser.add_argument('--ble', help='Serial number of device in advertised name of device for BLE communication')
parser.add_argument('--comport', help='Serial comport connected to the BLE uart gateway', required=False)
parser.add_argument('--rtt', help='Serial number of Segger J-Link to use for RTT communication')
parser.add_argument('--jlinkpath', help='Path to JLink executable')
parser.add_argument('--suid', help='DUT Unique serial number', required=False)
parser.add_argument('--pt', help='DUT Product type', required=False)
parser.add_argument('--ems', help='DUT EMS provider', required=False)
parser.add_argument('--server', help='DUT server to connect to', required=True)
parser.add_argument('--flash', help='If DUT should be flashed or not', required=True)
args = parser.parse_args()

# Build connection
if (args.ble and args.rtt):
	raise Exception("Can't connect to both BLE and RTT. Choose one!")
elif (not args.ble) and (not args.rtt):
	print("Didn't specify a connection. RTT will be used on any available J-Link devices.")


def flash_mcu(file, verbose=True) -> bool:
    cmd = ['nrfjprog', '--program', file, '--chiperase', '--verify', '--reset']
    if verbose: 
        print(' '.join(cmd))
    process = subprocess.Popen(
		cmd, 
		stdout=subprocess.PIPE, 
		stderr=subprocess.PIPE
	)
    out, err = map(lambda res: re.sub('[\r\n]+', '\r', res.decode('utf-8', 'ignore').strip()).split('\r'), process.communicate()) 

    if verbose: 
        for o in out: print(o)
    error = any('error' in e.lower() for e in err)
    if error or verbose: 
        for e in err: print(e)
    process.terminate()
    return not error

if not args.suid:
	while 1:
		suid = input("Enter serial number: ")
		if suid and len(suid) > 5:
			args.suid = suid.strip()
			break
		else:
			print(f'invalid serial number {suid}')


set_file = open(args.suid + "_" + str(int(time.time())) + ".log", "w")

if (args.flash == "y"):
	#file = 'C:\\Nofence\\firmware\\x3\\x3-fw-app\\merged_with_text_parser.hex'	
	file = '.\\merged.hex'
	print("Start flashing...")
	flash_ok = flash_mcu(file)
	print(flash_ok)
	set_file.write("SeggerFLASH = "  + str(flash_ok) + "\n")
	print("End")
	time.sleep(3);
else:
	set_file.write("SeggerFLASH = Not Flashed" + "\n")
	print("Not flashed")

stream = None
if args.ble:
	#stream = nfdiag.BLEStream("COM4", serial=args.ble)
	stream = nfdiag.BLEStream(port=args.comport, serial=args.ble)
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
#signal.signal(signal.SIGINT, signal_handler)

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

if (args.suid):
	if not cmndr.write_setting(nfdiag.ID_SERIAL, int(args.suid)):
		raise Exception("Failed to write settings")
if (args.ems):
	if not cmndr.write_setting(nfdiag.ID_EMS_PROVIDER, int(args.ems)):
		raise Exception("Failed to write settings")
if (args.pt):
	if not cmndr.write_setting(nfdiag.ID_PRODUCT_TYPE, int(args.pt)):
		raise Exception("Failed to write settings")

if(args.server=="Production"):
	if not cmndr.write_setting(nfdiag.ID_HOST_PORT, b"172.31.33.243:9876\x00"):
		raise Exception("Failed to write settings")
elif(args.server=="Staging"):	
	if not cmndr.write_setting(nfdiag.ID_HOST_PORT, b"172.31.36.11:4321\x00"):
		raise Exception("Failed to write settings")	
else:
	raise Exception("Unknown servername")	
if not cmndr.write_setting(nfdiag.ID_PRODUCT_RECORD_REV, 2):
	raise Exception("Failed to write settings")
if not cmndr.write_setting(nfdiag.ID_BOM_MEC_REV, 5):
	raise Exception("Failed to write settings")
if not cmndr.write_setting(nfdiag.ID_BOM_PCB_REV, 5):
	raise Exception("Failed to write settings")
if not cmndr.write_setting(nfdiag.ID_HW_VERSION, 21):		#20 = HW_Q Started a new range when making new HW with nRF52840
	raise Exception("Failed to write settings")

print("1: Serial No: " + str(cmndr.read_setting(nfdiag.ID_SERIAL)))
print("2: HOST PORT: " + str(cmndr.read_setting(nfdiag.ID_HOST_PORT)))
print("3: EMS Provider: " + str(cmndr.read_setting(nfdiag.ID_EMS_PROVIDER)))
print("4: Product type: " + str(cmndr.read_setting(nfdiag.ID_PRODUCT_TYPE)))
print("5: Generation: " + str(cmndr.read_setting(nfdiag.ID_PRODUCT_RECORD_REV)))
print("6: Model: " + str(cmndr.read_setting(nfdiag.ID_BOM_MEC_REV)))
print("7: PR Version: " + str(cmndr.read_setting(nfdiag.ID_BOM_PCB_REV)))
print("8: HW Version: " + str(cmndr.read_setting(nfdiag.ID_HW_VERSION)))
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
	#if failure:
		#raise Exception("!!!!TEST FAILED!!!!!")
else:
	raise Exception("No response when issuing test command")


#print("Start GNSS output each 2sec")

#gnss_struct_t = namedtuple('gnss_struct_t',['num_sv','pvt_flags','pvt_valid','overflow','x','y','height','speed','head_veh','h_dop','h_acc_dm','v_acc_dm','head_acc','lat','lon','updated_at','msss','ttff'])


# Read settings
#set_file = open(args.suid + "_" + str(int(time.time())) + ".log", "w")

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

buzzer_on = False
ep_on = False
output_on = False

def play_buzzer():
	while True:
		if buzzer_on:
			cmndr.start_buzzer()
			time.sleep(2)

def infinit_ep():
	zapcnt = 0
	while True:
		if ep_on:
			zapcnt = zapcnt + 1
			print("ZAP=" + str(zapcnt) + "\n")
			time.sleep(0.2)
			cmndr.electric_pulse_now()
			time.sleep(5.8)

oneshot = True
def output_data():
	while True:
		if output_on:
			if oneshot == True:
				set_file.write("UID - numSv,cno1,cno2,cno3,cno4,flags,valid,ovf,height,speed,hdop,lat,lon,msss,ttff,vbatt,isolar,vint,acc_x,acc_y,acc_z,temp,pres,hum"+ "\n")
				print("UID - numSv,cno1,cno2,cno3,cno4,flags,valid,ovf,height,speed,hdop,lat,lon,msss,ttff,vbatt,isolar,vint,acc_x,acc_y,acc_z,temp,pres,hum"+"\n")
			oneshot = False			
		elif output_on == False:
			oneshot = True	

		if output_on:
			try:		
				time.sleep(1)
				onboard_data = cmndr.get_onboard_data()

				value = struct.unpack("BBBBBBBBhHHiiIIIIIiiiiii", onboard_data["data"][0:68])
				print(str(cmndr.read_setting(nfdiag.ID_SERIAL)) + " - " + "{0}".format(value))
				set_file.write(str(cmndr.read_setting(nfdiag.ID_SERIAL)) + " - " + "{0}".format(value) + "\n")

			except KeyboardInterrupt:
				print("Loop stopped")
				set_file.write("Loop stopped" + "\n")
				break
			except Exception as e:
				print(f'error: {e}')			

buzzer_thread = Thread(target=play_buzzer, args=())
buzzer_thread.daemon = True
buzzer_thread.start()

infinit_ep_thread = Thread(target=infinit_ep, args=())
infinit_ep_thread.daemon = True
infinit_ep_thread.start()

output_thread = Thread(target=output_data, args=())
output_thread.daemon = True
output_thread.start()

run_thread = False
allow_fota = False
force_gnss_mode = 0
thread_flags = 0

while 1:
	print("Press following to run test")
	print("z - Start single EP test")
	print("Z - Start infinit EP test")
	print("I - Start infinit EP test runtime")
	print("O - Output debug data")
	print("s - Toggle 1V8_S voltage")
	print("c - Toggle onoff charging")
	print("a - Start EP and Toggle onoff charging")
	print("b - Start buzzer continous tone")
	print("SLEEP - Enter sleep")	
	print("ACT_FOTA - Enable FOTA")
	print("ACT_CEL - Start Cellular Thread")
	print("DACT_CEL_FOTA - Stopp Cellular Thread and FOTA")
	print("ACT_CEL_FOTA	- Start Cellular Thread and FOTA")
	print("FORCE_POLL_REQ - Force poll request")	
	print("GNSS_NOMODE - Release GNSS Force mode")
	print("GNSS_INACTIVE - Force GNSS to Inactive")
	print("GNSS_PSM - Force GNSS to PSM")
	print("GNSS_MAX - Force GNSS to MAX")
	x = input("'x' jump to next test step -> ")
	if x == "z":
		print("Pulse in 1 seconds!!!!!!!!!!")
		time.sleep(1)
		cmndr.electric_pulse_now()	
	elif x == "Z":
		print("Infinit EP each 5 second!!!!!!!!!!")
		ep_on = not ep_on
	elif x == "I":
		cmndr.electric_pulse_infinite(10)			
	elif x == "b":
		buzzer_on = not buzzer_on			
	elif x == "c":
		cmndr.turn_onoff_charging()	
	elif x == "s":
		cmndr.turn_onoff_1v8s()		
	elif x == "O":		
		output_on = not output_on		
	elif x == "a":
		print("Pulse in 1 seconds!!!!!!!!!!")
		time.sleep(1)
		cmndr.electric_pulse_now()		
		time.sleep(1)
		cmndr.turn_onoff_charging()	
	elif x == "ACT_CEL":		
		print("Start cellular thread")
		run_thread = True
		thread_flags = (force_gnss_mode << 2) | (allow_fota << 1) | (run_thread << 0)
		print("Threadflag = {0:b}".format(thread_flags))
		cmndr.thread_control(thread_flags)
	elif x == "DACT_CEL_FOTA":		
		print("Stop cellular thread")
		run_thread = False
		allow_fota = False
		thread_flags = (force_gnss_mode << 2) | (allow_fota << 1) | (run_thread << 0)
		print("Threadflag = {0:b}".format(thread_flags))
		cmndr.thread_control(thread_flags)
	elif x == "ACT_FOTA":		
		print("Activate FOTA")
		allow_fota = True
		thread_flags = (force_gnss_mode << 2) | (allow_fota << 1) | (run_thread << 0)
		print("Threadflag = {0:b}".format(thread_flags))
		cmndr.thread_control(thread_flags)
	elif x == "ACT_CEL_FOTA":		
		print("Activate CELLULAR AND FOTA")
		allow_fota = True		
		run_thread = True
		thread_flags = (force_gnss_mode << 2) | (allow_fota << 1) | (run_thread << 0)
		print("Threadflag = {0:b}".format(thread_flags))
		cmndr.thread_control(thread_flags)	
	elif x == "FORCE_POLL_REQ":		
		print("FORCE POLL REQ")
		cmndr.force_poll_req()			
	elif x == "GNSS_NOMODE":		
		print("GNSS NOMODE")
		force_gnss_mode = 0
		thread_flags = (force_gnss_mode << 2) | (allow_fota << 1) | (run_thread << 0)
		print("Threadflag = {0:b}".format(thread_flags))
		cmndr.thread_control(thread_flags)
	elif x == "GNSS_INACTIVE":				
		print("GNSS INACTIVE")
		force_gnss_mode = 1
		thread_flags = (force_gnss_mode << 2) | (allow_fota << 1) | (run_thread << 0)
		print("Threadflag = {0:b}".format(thread_flags))
		cmndr.thread_control(thread_flags)
	elif x == "GNSS_PSM":				
		print("GNSS PSM")
		force_gnss_mode = 2
		thread_flags = (force_gnss_mode << 2) | (allow_fota << 1) | (run_thread << 0)
		print("Threadflag = {0:b}".format(thread_flags))
		cmndr.thread_control(thread_flags)
	elif x == "GNSS_MAX":				
		print("GNSS MAX")
		force_gnss_mode = 4
		thread_flags = (force_gnss_mode << 2) | (allow_fota << 1) | (run_thread << 0)
		print("Threadflag = {0:b}".format(thread_flags))
		cmndr.thread_control(thread_flags)			
	elif x == "SLEEP":		
		cmndr.enter_sleep()
	elif x == "x":		
		break

#	GNSSMODE_NOMODE = 0,
#	GNSSMODE_INACTIVE = 1,
#	GNSSMODE_PSM = 2,
#	GNSSMODE_CAUTION = 3,
#	GNSSMODE_MAX = 4,
#	GNSSMODE_SIZE = 5

# Read CCID from modem
ccid = b""
timeout = time.time()+60
while len(ccid) == 0:
	ccid = cmndr.modem_get_ccid()
	if ccid is None:
		raise Exception("No response while reading CCID")		
	if time.time() > timeout:
		raise Exception("Timed out waiting for valid CCID. Modem failure!")
print("Modem CCID: " + str(int(ccid)))
set_file.write("Modem CCID = " + str(int(ccid)) + "\n")

while 1:
	x = input("Testing OK? 'y' or 'n'")
	if x == "y":
		print("Testeds OK!!" + "\n")
		set_file.write("Testeds OK!!" + "\n")
		break
	elif x == "n":
		print("Test FAILED!!" + "\n")
		set_file.write("Test FAILED!!" + "\n")
		break

set_file.close()

#cleanup_stop_thread()
sys.exit()
# Testing done, tear down
signal_handler(None, None)
