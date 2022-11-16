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


def try_stimuli_cmd(cmd, timeout=5):
    start_time = time.time()
    got_resp = False
    while time.time() < (start_time + timeout) and (not got_resp):
        resp = cmndr.send_cmd(nfdiag.GROUP_STIMULATOR, cmd)	
        if resp:
            got_resp = True
            print('Got response')
            return resp
    print('Did not get response...')
    return None




#print('resp code:', hex(resp['code']), 'data len:', len(resp['data']))

def read_onboard_data(resp):
	# onboard data: BBHHddd
	fields = [
		'num_sv',
		'cno_avg',
		'battery_mv',
		'charging_ma',
		'temp',	
		'humidity',	
		'pressure'	
	]
	values = struct.unpack('BBHHddd', resp['data'][0:32])
	onboard_data = {}
	for n,v in enumerate(values):
		onboard_data[fields[n]] = v

	return onboard_data
	

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

print(read_gnss_data(try_stimuli_cmd(0xA2)))

print(read_onboard_data(try_stimuli_cmd(0xA0)))

