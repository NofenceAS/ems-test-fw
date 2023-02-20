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
logger = logging.getLogger(__name__)


OPERATOR_MODE = False


cmndr = None
sn = None

def user_input(prompt):
    try:
        return input(prompt)
    except Exception as e:
        raise KeyboardInterrupt()

def run_command(group, cmd, payload=None, timeout=2):
    global cmndr
    resp = None
    resp_str = 'TIMEOUT'
    try:
        resp = cmndr.send_cmd(group, cmd, payload, timeout)
        if resp:
            if resp['code'] == nfdiag.RESP_ACK:
                resp_str = 'OK' 
            elif resp['code'] == nfdiag.RESP_DATA:
                resp_str = f'OK (data: {resp["data"]})'
            else:
                resp_str = f'FAILED (code: {resp["code"]})'
    except Exception as e:
        resp_str = f'DIAG COMMAND FAILED: ({e})'
    logger.debug(resp)
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
    ]
    if not OPERATOR_MODE:
        params += [
            ('Accel Sigma Noactivity Limit', nfdiag.ID_ACC_SIGMA_NOACT),
            ('Accel Sigma Sleep Limit', nfdiag.ID_ACC_SIGMA_SLEEP),
            ('Off Animal Time Limit', nfdiag.ID_OFF_ANIMAL_TIME),
            ('FLASH_TEACH_MODE_FINISHED', nfdiag.ID_FLASH_TEACH_MODE_FINISHED),
            ('FLASH_KEEP_MODE', nfdiag.ID_FLASH_KEEP_MODE),
            ('FLASH_ZAP_CNT_TOT', nfdiag.ID_FLASH_ZAP_CNT_TOT),
            ('FLASH_ZAP_CNT_DAY', nfdiag.ID_FLASH_ZAP_CNT_DAY),
            ('FLASH_WARN_CNT_TOT', nfdiag.ID_FLASH_WARN_CNT_TOT),
        ]

    for pname, pid in params:
        current_val = cmndr.read_setting(pid)
        val = input(f'\n{pname} [{current_val}]: ')
        if val:
            if 's' in pid[1]:
                val = bytes(val, 'utf-8', 'ignore') + b'\x00'
                print(val)
            else:
                val = int(val)
            if not cmndr.write_setting(pid, val):
                print(f'Error writing {val}')
            new_val = cmndr.read_setting(pid)
            print(f' - {pname} = "{new_val}"')

    if OPERATOR_MODE:
        # We don't want the operator to see these values, but we want them to have
        # a default value. Thus we program them silently at the end of this call 
        cmndr.write_setting(nfdiag.ID_ACC_SIGMA_NOACT, 400)
        cmndr.write_setting(nfdiag.ID_ACC_SIGMA_SLEEP, 600)
        cmndr.write_setting(nfdiag.ID_OFF_ANIMAL_TIME, 1800)
        cmndr.write_setting(nfdiag.ID_FLASH_TEACH_MODE_FINISHED, 0)
        cmndr.write_setting(nfdiag.ID_FLASH_KEEP_MODE, 0)
        cmndr.write_setting(nfdiag.ID_FLASH_ZAP_CNT_TOT, 0)
        cmndr.write_setting(nfdiag.ID_FLASH_ZAP_CNT_DAY, 0)
        cmndr.write_setting(nfdiag.ID_FLASH_WARN_CNT_TOT, 0)													 													   																								

def print_config():
    global cmndr, sn
    sn = cmndr.read_setting(nfdiag.ID_SERIAL)
    print("\n----FLASH CONTENT--------------------------------------------------------\n") 
    print(f'-  Serial No [RW]:                  {sn}')
    print(f'-  Host Port [RW]:                  {str(cmndr.read_setting(nfdiag.ID_HOST_PORT))}')
    print(f'-  EMS Provider [RW]:               {cmndr.read_setting(nfdiag.ID_EMS_PROVIDER)}')
    print(f'-  Product Type [RW]:               {cmndr.read_setting(nfdiag.ID_PRODUCT_TYPE)}')
    print(f'-  Generation [RW]:                 {cmndr.read_setting(nfdiag.ID_PRODUCT_RECORD_REV)}')
    print(f'-  Model [RW]:                      {cmndr.read_setting(nfdiag.ID_BOM_MEC_REV)}')
    print(f'-  PR Revision [RW]:                {cmndr.read_setting(nfdiag.ID_BOM_PCB_REV)}')
    print(f'-  PCB HW Version [RW]:             {cmndr.read_setting(nfdiag.ID_HW_VERSION)}')
    print(f'-  MCU FW Version:                  {cmndr.read_setting(nfdiag.ID_FW_VERSION)}')
    if not OPERATOR_MODE:
        read_flash_config()

def read_flash_config():
    global cmndr, sn
    sn = cmndr.read_setting(nfdiag.ID_SERIAL)
    diag_flags = read_flag_configuration()
    print(f'-  Diag flags:                      {bin(diag_flags).replace("0b","").zfill(8)}')          
    print(f'-  Accel Sigma Noactivity Limit:    {cmndr.read_setting(nfdiag.ID_ACC_SIGMA_NOACT)}')
    print(f'-  Accel Sigma Sleep Limit:         {cmndr.read_setting(nfdiag.ID_ACC_SIGMA_SLEEP)}')
    print(f'-  Off Animal Time Limit:           {cmndr.read_setting(nfdiag.ID_OFF_ANIMAL_TIME)}')
    print(f'-  TEACH_MODE_FINISHED:             {cmndr.read_setting(nfdiag.ID_FLASH_TEACH_MODE_FINISHED)}')
    print(f'-  KEEP_MODE:                       {cmndr.read_setting(nfdiag.ID_FLASH_KEEP_MODE)}')
    print(f'-  ZAP_CNT_TOT:                     {cmndr.read_setting(nfdiag.ID_FLASH_ZAP_CNT_TOT)}')
    print(f'-  ZAP_CNT_DAY:                     {cmndr.read_setting(nfdiag.ID_FLASH_ZAP_CNT_DAY)}')
    print(f'-  WARN_CNT_TOT:                    {cmndr.read_setting(nfdiag.ID_FLASH_WARN_CNT_TOT)}')
    print("\n----ONBOARD DEVICE VERSIONS--------------------------------------------------------\n")
    device_version = cmndr.get_device_version_data()
    device_version = ' '.join([v for v in str(device_version, 'utf8','ignore').split('\x00') if len(v)])
    print(f'Onboard Module Version [GNSS and Modem]:                {str(device_version)}')

run_thread = False
allow_fota = False
force_gnss_mode = 0
thread_flags = 0

def thread_control():
    global cmndr, run_thread, allow_fota, force_gnss_mode, thread_flags
    opt = [
        ('Start cellular thread', 'ACT_CEL'),
        ('Stop cellular thread', 'DACT_CEL_FOTA'),
        ('Activate FOTA', 'ACT_FOTA'),
        ('Activate CELLULAR AND FOTA', 'ACT_CEL_FOTA'),
        ('Set GNSS to NOMODE', 'GNSS_NOMODE'),
        ('Set GNSS to INACTIVE', 'GNSS_INACTIVE'),
        ('Set GNSS to PSM', 'GNSS_PSM'),
        ('Set GNSS to MAX', 'GNSS_MAX'),
    ]
    print(f'Current:\n thread_flags = {thread_flags:>08b}\n run_thread = {run_thread}\n allow_fota = {allow_fota}\n force_gnss_mode = {force_gnss_mode}\n')
    for n, o in enumerate(opt):
        print(f'{n}. {o[0]} ({o[1]})')
    cmd = user_input(f'\nSet thread to (0-{len(opt)-1}): ').strip()
    if cmd.isdigit() and (int(cmd) >= 0 and int(cmd) < len(opt)):

        str_cmd = opt[int(cmd)][1]

        if str_cmd == "ACT_CEL":		
            print("Start cellular thread")
            run_thread = True
        elif str_cmd == "DACT_CEL_FOTA":		
            print("Stop cellular thread")
            run_thread = False
            allow_fota = False
        elif str_cmd == "ACT_FOTA":		
            print("Activate FOTA")
            allow_fota = True
        elif str_cmd == "ACT_CEL_FOTA":		
            print("Activate CELLULAR AND FOTA")
            allow_fota = True		
            run_thread = True
        elif str_cmd == "GNSS_NOMODE":		
            print("GNSS NOMODE")
            force_gnss_mode = 0
        elif str_cmd == "GNSS_INACTIVE":				
            print("GNSS INACTIVE")
            force_gnss_mode = 1
        elif str_cmd == "GNSS_PSM":				
            print("GNSS PSM")
            force_gnss_mode = 2
        elif str_cmd == "GNSS_MAX":				
            print("GNSS MAX")
            force_gnss_mode = 4

        thread_flags = (force_gnss_mode << 2) | (allow_fota << 1) | (run_thread << 0)
        print(f'thread_flags = {thread_flags:>08b} (run_thread = {run_thread}, allow_fota = {allow_fota}, force_gnss_mode = {force_gnss_mode})')
        cmndr.thread_control(thread_flags)
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
    ]
    if not OPERATOR_MODE:
        opt += [
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


def debug_log():
    global cmndr
    resp_str, resp = run_command(nfdiag.GROUP_SYSTEM, nfdiag.CMD_LOG, b'\x01\x01')
    if resp and (resp['code'] == nfdiag.RESP_ACK or resp['code'] == nfdiag.RESP_DATA): 
        print(f'Enabling debug log: {resp_str} (ctrl+c to stop)\n---\n')
        while True:
            try:
                data = cmndr.get_data(0.1)
                if data:
                    print(str(data, 'ascii', 'ignore'))
            except KeyboardInterrupt:
                print('\n---\nStopping debug log...')
                break
    else:
        print(f'Enabling debug log: {resp_str}\n')
    _ = run_command(nfdiag.GROUP_SYSTEM, nfdiag.CMD_LOG, b'\x00\x00') #timeout first try
    resp_str, resp = run_command(nfdiag.GROUP_SYSTEM, nfdiag.CMD_LOG, b'\x00\x00')
    print(f'Disabling debug log: {resp_str}\n')




# ------------------------


# Parse input arguments
parser = argparse.ArgumentParser(description='Nofence final test')
parser.add_argument('--comport', help='Serial comport connected to the BLE uart gateway', required=False)
parser.add_argument('--rtt', help='Serial number of Segger J-Link to use for RTT communication', required=False)
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
            sys.exit('SN missing')

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
        sys.exit('Connecting attemt canceled...')
    except Exception as e:
        retries -= 1

try_until = time.time()+10
while not stream.is_connected():
    time.sleep(0.1)
    if time.time() > try_until:
        sys.exit('Timed out waiting for connection...')
        
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
        sys.exit('Could not ping collar...')


print(f'\n-- {f"Connected ".ljust(40, "-")}\n')
print_config()

options = [
    ('pulse', trigger_pulse),
    ('buzzer', trigger_buzzer),
    ('charging ...', set_charging),
    ('read onboard data', read_onboard_data),
    ('change config', change_config),
    ('read config', print_config),
    ('read CCID', read_modem_ccid),
    ('read IP', read_modem_ip),
    ('thread control ...', thread_control),
    ('config flags', set_flag_configuration),
    ('clear flags', clear_all_flags, OPERATOR_MODE),
    ('clear flash memory ...', clear_flash),
    ('sleep mode', force_sleep),
    ('self-test', self_test),
    ('force poll request', force_poll_req),
    ('read flash config', read_flash_config),
    ('debug log', debug_log),
]

while True:
    try:
        print(f'\n{"-"*43}\n')
        for n, opt in enumerate(options):
            if len(opt) < 3 or (len(opt) >= 3 and not opt[2]): 
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
                    c = options[n][0]                    
                    func = options[n][1]
                    hidden = options[n][2] if len(options[n]) >= 3 else False
                    if not hidden:
                        print(f'-- {f"{c.title()} ".ljust(40, "-")}\n')
                        func()
                    else:
                        print(f'{cmd} not allowed')
                else:
                    print(f'Invalid command: "{cmd}"')
            except Exception as e:
                print(f'Error cmd: {e}')
    except KeyboardInterrupt:
        stream.close()
        print('\nQuitting...')
        break
    #except Exception as e:
    #    print(f'Error sys: {e}')
    #    time.sleep(2)
            

