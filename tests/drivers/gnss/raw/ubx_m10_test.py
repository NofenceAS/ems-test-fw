import os
import sys

UBX_SYNC_CHAR_1 = 0xB5
UBX_SYNC_CHAR_2 = 0x62

# "B5 62 0A 08 1C 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 00 00 00 00 00 06 00 01 06 00 00 3C C6"

def calc_chk(data):
    ck_a = 0
    ck_b = 0
    for i in range(len(data)):
        ck_a = (ck_a + data[i]) & 0xFF
        ck_b = (ck_b + ck_a) & 0xFF
    
    return (ck_a, ck_b)

#0xB5, 0x62, 0x06, 0x04, 0x04, 0x00, 0x01, 0x00, 0x01, 0x00
payload = [0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x52, 0x40, 0x00, 0xC2, 0x01, 0x00]#list(range(76)) # [0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x21, 0x30, 0x39, 0x05]
msg_class = 0x06
msg_id = 0x8b
raw_data = [UBX_SYNC_CHAR_1, UBX_SYNC_CHAR_2, msg_class, msg_id, len(payload)&0xFF, (len(payload)>>8)&0xFF] + payload

(ck_a, ck_b) = calc_chk(raw_data[2:])

raw_data += [ck_a, ck_b]

print('[{}]'.format(', '.join(hex(x) for x in raw_data)))