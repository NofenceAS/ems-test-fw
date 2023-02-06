import sys
import os
import time
import random
from datetime import datetime

base_path = os.path.dirname(os.path.abspath(__file__))
lib_path = os.path.join(base_path, "..")
sys.path.append(lib_path)

import nfdiag
import signal
import time
import argparse
import struct

import cobs

import logging
logging.basicConfig(level=logging.DEBUG)



cmndr = None
sn = None

def user_input(prompt):
    try:
        return input(prompt)
    except Exception as e:
        raise KeyboardInterrupt()

def run_command(group, cmd, payload=None):
    global cmndr
    resp = None
    resp_str = 'TIMEOUT'
    try:
        resp = cmndr.send_cmd(group, cmd, payload)
        if resp:
            if resp['code'] == nfdiag.RESP_ACK:
                resp_str = 'OK' 
            elif resp['code'] == nfdiag.RESP_DATA:
                resp_str = f'OK (data: {resp["data"]})'
            else:
                resp_str = f'FAILED (code: {resp["code"]})'
    except Exception as e:
        resp_str = f'FAILED ({e})'
    logging.debug(resp)
    return resp_str, resp

# ---------------

def trigger_pulse():
    global cmndr
    print('Triggering puls:', end=' '),
    time.sleep(1)
    resp_str, _ = run_command(nfdiag.GROUP_STIMULATOR, nfdiag.CMD_ELECTRICAL_PULSE)
    print(resp_str)

def trigger_buzzer():
    global cmndr
    print('Triggering buzzer:', end=' '),
    resp_str, _ = run_command(nfdiag.GROUP_STIMULATOR, nfdiag.CMD_BUZZER_WARN)
    print(resp_str)

def set_charging():
    global cmndr
    opt = [
        ('Disable charging', 'OFF'),
        ('Enable charging', 'ON'),
        ('Toggle charging', 'TURN_ONOFF'),
    ]
    for n, o in enumerate(opt):
        print(f'{n}. {o[0]} ({o[1]})')
    cmd = user_input(f'\nSet charging (0-{len(opt)-1}): ').strip()
    if cmd.isdigit() and (int(cmd) >= 0 and int(cmd) < len(opt)):    
        c = int(cmd)
        print(f'Charging = {opt[c][1]}:', end=' ')
        if c < 2:
            resp_str, _ = run_command(nfdiag.GROUP_STIMULATOR, nfdiag.CMD_SET_CHARGING_EN, b'\x01' if c else b'\x00')
        else:
            resp_str, _ = run_command(nfdiag.GROUP_STIMULATOR, nfdiag.CMD_TURN_ONOFF_CHARGING)
        print(resp_str)
    else:
        print(f'canceled')

def read_modem_ccid():
    global cmndr
    ccid = b''
    timeout = time.time()+60
    while len(ccid) == 0:
        ccid = cmndr.modem_get_ccid()
        if ccid is None:
            raise Exception('No response while reading CCID')		
        if time.time() > timeout:
            raise Exception('Timed out waiting for valid CCID')
    print(f'Modem CCID: {int(ccid)}')


def read_modem_ip():
    global cmndr
    ip = b''
    timeout = time.time()+60
    while len(ip) == 0:
        ip = cmndr.modem_get_ip()
        if ip is None:
            raise Exception('No response while reading IP')		
        if time.time() > timeout:
            raise Exception('Timed out waiting for valid IP')
    print(f'Modem IP: {ip}')


def read_onboard_data():
    global cmndr
    num_errors = 0
    num_requests = 0
    max_requests = 10
    inf_request = False
    try: 
        req = input(f'Num requests (0 for inf) [{max_requests}]: ').strip()
        if req:
            max_requests = int(req)
    except:
        raise Exception('input error')
    if max_requests == 0:
        inf_request = True
    print('\nctrl+c to stop\n')        
    obdata_cols = 'timestamp,numSv,cno1,cno2,cno3,cno4,flags,valid,ovf,height,speed,hdop,lat,lon,msss,ttff,vbatt,isolar,vint,acc_x,acc_y,acc_z,temp,pres,hum'
    cols = obdata_cols.split(',')
    cols_size = [2]*(len(cols)+10)
    cols_size[0] = 18
    while inf_request or num_requests < max_requests:
        try:		
            time.sleep(0.25)
            resp = cmndr.send_cmd(nfdiag.GROUP_STIMULATOR, nfdiag.CMD_GET_ONBOARD_DATA)
            if resp['code'] == nfdiag.RESP_DATA:
                num_errors = 0
                if num_requests % 30 == 0:
                    colvals = []
                    for n, c in enumerate(cols):
                        cols_size[n] = max(len(c), cols_size[n])
                        colvals.append(f'{c.rjust(cols_size[n])}')
                    print(', '.join(colvals))
                num_requests += 1
                values = [datetime.now().strftime('%d-%m-%y %H:%M:%S')]
                values += struct.unpack('BBBBBBBBhHHiiIIIIIiiiiii', resp['data'][0:68])
                values[-1] /= 1000. # convert int32 temperature back to float 
                values[-2] /= 1000. # convert int32 pressure back to float
                values[-3] /= 1000. # convert int32 humidity back to float
                obvals = []
                for n, v in enumerate(values):
                    val = str(v)
                    cols_size[n] = max(len(val), cols_size[n])
                    obvals.append(f'{val.rjust(cols_size[n])}')
                print(', '.join(obvals))
        except KeyboardInterrupt:
            print('Stopping diag requests')
            break
        except Exception as e:
            num_errors += 1
            if num_errors >= 3:
                raise Exception(e)
    
def change_config():
    global cmndr
    
    params = [
        ('Serial number', nfdiag.ID_SERIAL),
        ('Host Port', nfdiag.ID_HOST_PORT),
        ('EMS Provider', nfdiag.ID_EMS_PROVIDER),
        ('Product Type', nfdiag.ID_PRODUCT_TYPE),
        ('Generation', nfdiag.ID_PRODUCT_RECORD_REV),
        ('Model', nfdiag.ID_BOM_MEC_REV),
        ('Revision', nfdiag.ID_BOM_PCB_REV),
        ('HW Version', nfdiag.ID_HW_VERSION),
        ('Accel sigma noactivity limit', nfdiag.ID_ACC_SIGMA_NOACT)
        ('Accel sigma sleep limit', nfdiag.ID_ACC_SIGMA_SLEEP)
        ('Off animal time limit', nfdiag.ID_OFF_ANIMAL_TIME)
    ]

    for pname, pid in params:
        current_val = cmndr.read_setting(pid)
        val = input(f'\n{pname} [{current_val}]: ')
        if val:
            if 's' in pid[1]:
                val = bytes(val, 'utf-8', 'ignore') + b'\x00'
            else:
                val = int(val)
            if not cmndr.write_setting(pid, val):
                print(f'Error writing {val}')
            new_val = cmndr.read_setting(pid)
            print(f' - {pname} = "{new_val}"')


def print_config():
    global cmndr, sn
    sn = cmndr.read_setting(nfdiag.ID_SERIAL)
    print(f'-  Serial No:    {sn}')
    print(f'-  Host Port:    {str(cmndr.read_setting(nfdiag.ID_HOST_PORT))}')
    print(f'-  EMS Provider: {cmndr.read_setting(nfdiag.ID_EMS_PROVIDER)}')
    print(f'-  Product Type: {cmndr.read_setting(nfdiag.ID_PRODUCT_TYPE)}')
    print(f'-  Generation:   {cmndr.read_setting(nfdiag.ID_PRODUCT_RECORD_REV)}')
    print(f'-  Model:        {cmndr.read_setting(nfdiag.ID_BOM_MEC_REV)}')
    print(f'-  Revision:     {cmndr.read_setting(nfdiag.ID_BOM_PCB_REV)}')
    print(f'-  HW Version:   {cmndr.read_setting(nfdiag.ID_HW_VERSION)}')
    diag_flags = read_flag_configuration()
    print(f'\n-  Diag flags:   {bin(diag_flags).replace("0b","").zfill(8)}')


def thread_control():
    global cmndr
    opt = [
        ('Stop cellular thread', 'STOP CEL'),
        ('Start cellular thread', 'ACT_CEL'),
        ('Activate FOTA', 'ACT_FOTA'),
        ('Activate CELLULAR AND FOTA', 'ACT_CEL_FOTA')
    ]
    for n, o in enumerate(opt):
        print(f'{n}. {o[0]} ({o[1]})')
    cmd = user_input(f'\nSet thread to (0-{len(opt)-1}): ').strip()
    if cmd.isdigit() and (int(cmd) >= 0 and int(cmd) < len(opt)):    
        cmndr.thread_control(int(cmd))
        print(f'setting thread to {cmd} ({opt[int(cmd)][1]})')
    else:
        print(f'canceled')


def force_sleep():
    global cmndr
    print('Forcing sleep mode')
    cmndr.enter_sleep()


def self_test():
    global cmndr
    # Running test
    resp = cmndr.send_cmd(nfdiag.GROUP_SYSTEM, nfdiag.CMD_TEST, b"")

    # Interpret result
    SELFTEST_FLASH_POS = 0
    SELFTEST_EEPROM_POS = 1
    SELFTEST_ACCELEROMETER_POS = 2
    SELFTEST_GNSS_POS = 3

    selftests = [
        (SELFTEST_FLASH_POS, "Flash"), 
        (SELFTEST_EEPROM_POS, "EEPROM"), 
        (SELFTEST_ACCELEROMETER_POS, "Accelerometer"),
        (SELFTEST_GNSS_POS, "GNSS")
    ]

    failure = False
    if resp and resp['code'] == nfdiag.RESP_DATA:
        test_done, test_passed = struct.unpack("<II", resp["data"])
        print("Tests done = " + "{0:b}".format(test_done))
        print("Tests passed = " + "{0:b}".format(test_passed))
        for selftest in selftests:
            if test_done & (1 << selftest[0]) == 0:
                failure = True
                print("Did not run test: " + selftest[1])
            if test_passed & (1 << selftest[0]) == 0:
                failure = True
                print("Failed test: " + selftest[1])
        if failure:
            print('self-test failed')
    else:
        raise Exception("No response when issuing test command")


def force_poll_req():
    global cmndr
    print('Forcing poll request:', end=' ')
    resp_str, _ = run_command(nfdiag.GROUP_SYSTEM, nfdiag.FORCE_POLL_REQ)
    print(resp_str)



def read_flag_configuration():
    _, resp = run_command(nfdiag.GROUP_SYSTEM, nfdiag.GET_DIAG_FLAGS)
    diag_flags = 0
    try:
        if resp['code'] == 1:
            value = struct.unpack('<I', resp['data'][:4])
            diag_flags = value[0]
    except Exception as e:
        print(f'Error reading flags: {e}')
    return diag_flags

def clear_all_flags():
    diag_flags = read_flag_configuration()
    print(f'Current diagnostic flags: {bin(diag_flags).replace("0b","").zfill(8)}')
    print('Clearing', end=' ')
    resp_str, resp = run_command(nfdiag.GROUP_SYSTEM, nfdiag.CLR_DIAG_FLAGS)
    print(resp_str) 
    diag_flags = read_flag_configuration()
    print(f'Updated diagnostic flags: {bin(diag_flags).replace("0b","").zfill(8)}')
   

def set_flag_configuration():
    global cmndr

    flaglist = [
        ('FOTA_DISABLED', nfdiag.FLAG_FOTA_DISABLED),
        ('CELLULAR_THREAD_DISABLED', nfdiag.FLAG_CELLULAR_THREAD_DISABLED),
    ]
    diag_flags = read_flag_configuration()
    print(f'Current diagnostic flags: {bin(diag_flags).replace("0b","").zfill(8)}\n')
    for fname, fval in flaglist:
        current_val = 1 if fval & diag_flags else 0
        val = input(f'{fname} [{current_val}]: ')
        if val == '1' or val == '0':
            try: 
                payload = struct.pack('<I', fval)
                if val == '1':
                    _, resp = run_command(nfdiag.GROUP_SYSTEM, nfdiag.SET_DIAG_FLAGS, payload)
                else:
                    _, resp = run_command(nfdiag.GROUP_SYSTEM, nfdiag.CLR_DIAG_FLAGS, payload)
                if resp['code'] == 1:
                    value = struct.unpack('<I', resp['data'][:4])
                    new_val = current_val = 1 if fval & value[0] else 0
                    print(f' - {fname} = "{new_val}"')
            except Exception as e:
                print(f'Error writing flag {val} for {fname}: {e}')
    
    diag_flags = read_flag_configuration()
    print(f'\nUpdated diagnostic flags: {bin(diag_flags).replace("0b","").zfill(8)}')


def clear_flash():
    global cmndr
    opt = [
        ('Pasture / fence version', nfdiag.CMD_CLEAR_PASTURE),
        ('All flash memory', nfdiag.CMD_ERASE_FLASH),
    ]
    print('Clear flash:')
    for n, o in enumerate(opt):
        print(f'{n}. {o[0]}')
    cmd = user_input(f'\nClear (0-{len(opt)-1}): ').strip()
    if cmd.isdigit() and (int(cmd) >= 0 and int(cmd) < len(opt)):            
        resp_str, resp = run_command(nfdiag.GROUP_SYSTEM, opt[int(cmd)][1])
        print(f'Cleared {opt[int(cmd)][0].lower()}: {resp_str}')
    else:
        print(f'canceled')




# ------------------------


# Parse input arguments
parser = argparse.ArgumentParser(description='Nofence final test')
parser.add_argument('--comport', help='Serial comport connected to the BLE uart gateway', required=False)
parser.add_argument('--rtt', help='Serial number of Segger J-Link to use for RTT communication', default='821007298', required=False)
parser.add_argument('--sn', help='Collar serial number or device name', required=False)
args = parser.parse_args()

sn = args.sn if args.sn else None
if args.comport:
    while not sn:
        try:
            sn = user_input('Enter serial number or device name to search for: ').strip()
            if len(sn) > 1:
                print(f'SN: "{sn}"')
                break
            else:
                print(f'Invalid SN: "{sn}"')
        except Exception as e:
            exit('SN missing')

stream = None
retries = 5
while retries > 0:
    try:
        if args.comport:
            print(f'Searching for "{sn}" BLE...')
            stream = nfdiag.BLEStream(port=args.comport, serial=sn)
            break
        else:
            stream = nfdiag.JLinkStream(serial=args.rtt)
            break
    except KeyboardInterrupt:
        exit('Connecting attemt canceled...')
    except Exception as e:
        retries -= 1

try_until = time.time()+10
while not stream.is_connected():
    time.sleep(0.1)
    if time.time() > try_until:
        exit('Timed out waiting for connection...')
        
got_ping = True
if stream.is_connected():
    cmndr = nfdiag.Commander(stream)
    print('Trying to ping...')
    got_ping = False
    try_until = time.time()+10
    while time.time() < try_until and not got_ping:
        resp = cmndr.send_cmd(nfdiag.GROUP_SYSTEM, nfdiag.CMD_PING)
        if resp:
            got_ping = True
            print("Got ping")
    if not got_ping:
        exit('Could not ping collar...')


print(f'\n-- {f"Connected ".ljust(40, "-")}\n')
print_config()

options = [
    ('pulse', trigger_pulse),
    ('buzzer', trigger_buzzer),
    ('charging', set_charging),
    ('read onboard data', read_onboard_data),
    ('change config', change_config),
    ('read config', print_config),
    ('read CCID', read_modem_ccid),
    ('read IP', read_modem_ip),
    ('thread control', thread_control),
    ('config flags', set_flag_configuration),
    ('clear flags', clear_all_flags),
    ('clear flash memory', clear_flash),
    ('sleep mode', force_sleep),
    ('self-test', self_test),
    ('force poll request', force_poll_req),
]

while True:
    try:
        print(f'\n{"-"*43}\n')
        for n, opt in enumerate(options):
            print(f'{str(n+1).rjust(2)}. {opt[0]}')
        print('\n q. quit (ctrl+c)')
        cmd = user_input(f'\nCommand (1-{len(options)}): ').strip()
        print('\n')
        if cmd.startswith('q'):
            raise KeyboardInterrupt()
        elif len(cmd):
            try:
                n = int(cmd)-1
                if n >= 0 and n < len(options):
                    c, func = options[n]
                    print(f'-- {f"{c.title()} ".ljust(40, "-")}\n')
                    func()
                else:
                    print(f'Invalid command: "{cmd}"')
            except Exception as e:
                print(f'Error cmd: {e}')
    except KeyboardInterrupt:
        print('\nQuitting...')
        break
    #except Exception as e:
    #    print(f'Error sys: {e}')
    #    time.sleep(2)
            

