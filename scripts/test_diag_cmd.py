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

def try_cmd(group, cmd, data=None, timeout=5):
    start_time = time.time()
    got_resp = False
    while time.time() < (start_time + timeout) and (not got_resp):
        resp = cmndr.send_cmd(group, cmd, data)	
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

#payload = struct.pack('<B', 1)
#resp = try_system_cmd(0x42)
#value = struct.unpack('I', resp['data'][:4])
#print(resp)

resp = try_cmd(nfdiag.GROUP_SYSTEM, 0x22)
#value = struct.unpack('I', resp['data'][:4])
print(resp)


# \x00\x74\x0a\x0a\x08\x96\xd7\xc3\x0a\x10\x00\x18\x88\x07\x12\x66\x08\x01\x18\x03\x20\x05\x28\x02\x30\xff\xff\x03\x38\xf3\x02\x42\x03\x30\x88\x07\x5a\x14\x38\x39\x34\x36\x32\x30\x33\x38\x30\x30\x37\x30\x30\x36\x38\x38\x37\x36\x39\x36\x68\x00\x92\x01\x06\x08\x14\x10\x00\x18\x02\xa0\x01\xff\xff\x03\xba\x01\x08\x08\x01\x10\x04\x18\x05\x20\x02\xc0\x01\xff\xff\x03\xc8\x01\xff\xff\x03\xda\x01\x08\x81\xb4\x1a\x2a\x89\x37\xeb\x85\xe2\x01\x09\x0a\x05\x32\x34\x32\x30\x31\x10\x07
