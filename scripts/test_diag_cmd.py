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


resp = try_stimuli_cmd(0xA0)

print('resp code:', hex(resp['code']), 'data len:', len(resp['data']))

print(resp)

value = struct.unpack('BBHHddd', resp['data'][0:32])
print(value)


