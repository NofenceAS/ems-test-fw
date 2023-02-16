import sys
import time
import logging
from queue import Queue, Empty
from pc_ble_driver_py.observers import *
from datetime import datetime  

CONNECTIONS = 1
CFG_TAG = 1

import logging
logging.basicConfig(level=logging.CRITICAL)

from cobs import cobs

def init(conn_ic_id):
    # noinspection PyGlobalUndefined
    global config, BLEDriver, BLEAdvData, BLEEvtID, BLEAdapter, BLEEnableParams, BLEGapTimeoutSrc, BLEUUID, BLEUUIDBase, BLEConfigCommon, BLEConfig, BLEConfigConnGatt, BLEGapScanParams
    from pc_ble_driver_py import config

    config.__conn_ic_id__ = conn_ic_id
    # noinspection PyUnresolvedReferences
    from pc_ble_driver_py.ble_driver import (
        BLEDriver,
        BLEAdvData,
        BLEEvtID,
        BLEEnableParams,
        BLEGapTimeoutSrc,
        BLEUUID,
        BLEUUIDBase,
        BLEGapScanParams,
        BLEConfigCommon,
        BLEConfig,
        BLEConfigConnGatt,
    )

    # noinspection PyUnresolvedReferences
    from pc_ble_driver_py.ble_adapter import BLEAdapter

    global nrf_sd_ble_api_ver
    nrf_sd_ble_api_ver = config.sd_api_ver_get()


class NFDiagnostics(BLEDriverObserver, BLEAdapterObserver):
    def __init__(self, adapter):
        super(NFDiagnostics, self).__init__()
        self.adapter = adapter
        self.conn_q = Queue()
        self.rsp_q = Queue()
        self.adapter.observer_register(self)
        self.adapter.driver.observer_register(self)
        self.adapter.default_mtu = 250
        self.BASE_UUID = BLEUUIDBase([0x6e, 0x40, 0x00, 0x00, 0xb5, 0xa3, 0xf3, 0x93, 0xe0, 0xa9, 0xe5, 0x0e, 0x24, 0xdc, 0xca, 0x9e])
        #self.BASE_UUID = BLEUUIDBase([0x6e, 0x40, 0x00, 0x01, 0xb5, 0xa3, 0xf3, 0x93, 0xe0, 0xa9, 0xe5, 0x0e, 0x24, 0xdc, 0xca, 0x9e])
        self.TX_UUID = BLEUUID(0x0003, self.BASE_UUID)
        self.RX_UUID = BLEUUID(0x0002, self.BASE_UUID)

    def open(self):
        self.adapter.driver.open()

        gatt_cfg = BLEConfigConnGatt()
        gatt_cfg.att_mtu = self.adapter.default_mtu
        gatt_cfg.tag = CFG_TAG
        self.adapter.driver.ble_cfg_set(BLEConfig.conn_gatt, gatt_cfg)

        self.adapter.driver.ble_enable()

    def close(self):
        print("Closing")
        self.adapter.close()

    def discover(self):
        scan_duration = 3
        params = BLEGapScanParams(interval_ms=500, window_ms=150, timeout_s=scan_duration)
        self.adapter.driver.ble_gap_scan_start(scan_params=params)

        try:
            new_conn = self.conn_q.get(timeout=scan_duration)
            return new_conn
        except Exception as e:
            pass


    def send_cmd(self, conn, cmd):
        self.adapter.write_req(
            conn, self.RX_UUID, cobs.encode(cmd) + b"\x00"
        )
    
    def get_rsp(self):
        return self.rsp_q.get(block=False)

    def on_gap_evt_connected(
        self, ble_driver, conn_handle, peer_addr, role, conn_params
    ):
        print("New connection: {}".format(conn_handle))
        self.conn_q.put(conn_handle)

    def on_gap_evt_disconnected(self, ble_driver, conn_handle, reason):
        print("Disconnected: {} {}".format(conn_handle, reason))

    def on_gap_evt_adv_report(self, ble_driver, conn_handle, peer_addr, rssi, adv_type, adv_data):
        if BLEAdvData.Types.complete_local_name in adv_data.records:
            dev_name_list = adv_data.records[BLEAdvData.Types.complete_local_name]
        elif BLEAdvData.Types.short_local_name in adv_data.records:
            dev_name_list = adv_data.records[BLEAdvData.Types.short_local_name]
        else:
            return

        dev_name = "".join(chr(e) for e in dev_name_list)
        address_string = "".join("{0:02X}".format(b) for b in peer_addr.addr)
        str_date_time = datetime.now().strftime("%d-%m-%Y %H:%M:%S")

        if dev_name.startswith('NF'): # and '?' not in dev_name:
            sn = dev_name[2:]
            print(f'[{str_date_time}] {sn} ({dev_name}, RSSI: {rssi}, address: 0x{address_string})')
            #self.adapter.connect(peer_addr, tag=CFG_TAG)

    def on_notification(self, ble_adapter, conn_handle, uuid, data):
        #if len(data) > 32:
        #    data = "({}...)".format(data[0:10])
        print("Connection: {}, {} = {}".format(conn_handle, uuid, data))

        if data[-1] == 0:
            print(data)
            rsp = cobs.decode(bytes(data[:-1]))
            print(rsp)
            self.rsp_q.put(rsp)

def main(selected_serial_port):
    print("Serial port used: {}".format(selected_serial_port))
    driver = BLEDriver(
        serial_port=selected_serial_port, auto_flash=False, baud_rate=1000000, log_severity_level="info"
    )

    adapter = BLEAdapter(driver)
    nfdiag = NFDiagnostics(adapter)
    nfdiag.open()
    print('Searching for BLE device names starting with "NF"... (Ctrl+C to stop)')
    while 1:
        try:
            conn = nfdiag.discover()
        except KeyboardInterrupt:
            print("Scanning stopped")
            break
        except Exception as e:
            print(f'error: {e}')

    nfdiag.close()


def item_choose(item_list):
    for i, it in enumerate(item_list):
        print("\t{} : {}".format(i, it))
    print(" ")

    while True:
        try:
            choice = int(input("Enter your choice: "))
            if (choice >= 0) and (choice < len(item_list)):
                break
        except Exception:
            pass
        print("\tTry again...")
    return choice


if __name__ == "__main__":
    logging.basicConfig(
        level="DEBUG",
        format="%(asctime)s [%(thread)d/%(threadName)s] %(message)s",
    )
    serial_port = None
    init('NRF52')
    descs = BLEDriver.enum_serial_ports()
    serial_port = None
    if len(descs) >= 2:
        choices = ["{}: {}".format(d.port, d.serial_number) for d in descs]
        choice = item_choose(choices)
        serial_port = descs[choice].port
    elif len(descs) == 1:
        serial_port = descs[0].port

    if serial_port:
        main(serial_port)
    else:
        print('No serial port found')
    quit()