import sys
import os

import nfdiag
import signal
import time
import argparse
import struct

# Parse input arguments
stream = nfdiag.JLinkStream(serial='821007298')

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


def try_stimuli_cmd(cmd, data=None, timeout=5):
    start_time = time.time()
    got_resp = False
    while time.time() < (start_time + timeout) and (not got_resp):
        resp = cmndr.send_cmd(nfdiag.GROUP_STIMULATOR, cmd, data)	
        if resp:
            got_resp = True
            print('Got response')
            return resp
    print('Did not get response...')
    return None

def try_system_cmd(cmd, data=None, timeout=5):
    start_time = time.time()
    got_resp = False
    while time.time() < (start_time + timeout) and (not got_resp):
        resp = cmndr.send_cmd(nfdiag.GROUP_SYSTEM, cmd, data)	
        if resp:
            got_resp = True
            print('Got response')
            return resp
    print('Did not get response...')
    return None


#print('resp code:', hex(resp['code']), 'data len:', len(resp['data']))

def read_onboard_data(resp):
	# onboard data: BBiBHHddd
	fields = [
		'num_sv',
		'cno_avg',
		'rssi',
		'pwr_state',
		'battery_mv',
		'charging_ma',
		'temp',	
		'humidity',	
		'pressure'	
	]
	# BBiBHHddd
	values = struct.unpack('BBiBHHddd', resp['data'][0:40])
	onboard_data = {}
	for n,v in enumerate(values):
		onboard_data[fields[n]] = v

	return onboard_data

def read_all_onboard_data(resp):
	# 'old' onboard data: BBBBBBBBhHHiiIIIIIiiiiii
	fields = [
		'numSv','cno1','cno2','cno3','cno4','flags','valid','ovf','height','speed',
		'hdop','lat','lon','msss','ttff','vbatt','isolar','vint',
		'acc_x','acc_y','acc_z','temp','pres','hum'
	]
	values = struct.unpack('BBBBBBBBhHHiiIIIIIiiiiii', resp['data'][0:68])
	all_onboard_data = {}
	for n,v in enumerate(values):
		all_onboard_data[fields[n]] = v

	return all_onboard_data

def read_gnss_data(resp):
	#gnss_data: iihhBhHhHHHHBBBBIII
	fields = [
		'lat', 'lon',
		'corr_x', 'corr_y',
		'overflow', 'height', 'speed_cms', 'head_veh',
		'h_dop', 'h_acc_dm', 'v_acc_dm', 'head_acc',
		'num_sv', 'cno_sat27', 'cno_min', 'cno_max', 'cno_avg',
		'pvt_flags', 'pvt_valid', 'updated_at', 'msss', 'ttff'
	]
	values = struct.unpack('iihhBhHhHHHHBBBBBBBIII', resp['data'][0:48])
	gnss_data = {}
	for n,v in enumerate(values):
		gnss_data[fields[n]] = v

	return gnss_data


def read_gsm_data(resp):
	#gsm_data: iiiii20s
	fields = [
		'rat', 'mnc',
		'rssi', 'min_rssi', 'max_rssi',
		'ccid'
	]
	values = struct.unpack('iiiii20s', resp['data'][0:40])
	gsm_data = {}
	for n,v in enumerate(values):
		gsm_data[fields[n]] = v

	return gsm_data


GET_GNSS_DATA = 0xA4
GET_OB_DATA = 0xA2
GET_ONBOARD_DATA = 0xA0
GET_GSM_DATA = 0xA6

#print(read_gnss_data(try_stimuli_cmd(GET_GNSS_DATA)))

#print(read_onboard_data(try_stimuli_cmd(GET_OB_DATA)))

#print(read_all_onboard_data(try_stimuli_cmd(GET_ONBOARD_DATA)))

#print(read_gsm_data(try_stimuli_cmd(GET_GSM_DATA)))


#payload = struct.pack('<I', 2000)
#resp = try_stimuli_cmd(0xB0, payload)
#value = struct.unpack('I', resp['data'][:4])
#print(resp)
#print(value#)

payload = struct.pack('<B', 1)
resp = try_system_cmd(0x42)
#value = struct.unpack('I', resp['data'][:4])
print(resp)
