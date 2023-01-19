from cobs import cobs
import struct
from queue import Queue, Empty
import crc
import threading
import time
from collections import namedtuple

import logging
logging.basicConfig(level=logging.INFO)

# Definitions used for group/command/response
GROUP_SYSTEM = 0x00
CMD_PING = 0x55
CMD_LOG = 0x70
CMD_TEST = 0x7E
CMD_REPORT = 0x5E
CMD_THREAD_CONTROL = 0x40
CMD_FORCE_POLL_REQ = 0x42
CMD_ENTER_SLEEP = 0xE0
CMD_WAKEUP = 0xE0
CMD_REBOOT = 0xEB

#CMD_POWER_PROFILE_GNSS_MAX = 0x42
#CMD_POWER_PROFILE_GNSS_POT = 0x43
#CMD_POWER_PROFILE_GNSS_TRACKING = 0x44
#CMD_POWER_PROFILE_GNSS_ACQUSITION = 0x45
#CMD_POWER_PROFILE_GNSS_PSM = 0x46
#CMD_POWER_PROFILE_MCU_SLEEP = 0x47

GROUP_SETTINGS = 0x01
ID_SERIAL = (0x00, "I")
ID_HOST_PORT = (0x01, "s")
ID_EMS_PROVIDER = (0x02, "B")
ID_PRODUCT_RECORD_REV = (0x03, "B")
ID_BOM_MEC_REV = (0x04, "B")
ID_BOM_PCB_REV = (0x05, "B")
ID_HW_VERSION = (0x06, "B")
ID_PRODUCT_TYPE = (0x07, "H")
CMD_READ = 0x00
CMD_WRITE = 0x01
ERASE_ALL = 0xEA

GROUP_STIMULATOR = 0x02

CMD_GET_ONBOARD_DATA = 0xA0
CMD_BUZZER_WARN = 0xB0
CMD_ELECTRICAL_PULSE = 0xE0
CMD_TURN_ONOFF_CHARGING = 0xE1
CMD_SET_CHARGING_EN = 0xC0
#CMD_TURN_ONOFF_1V8S = 0xE2
CMD_ELECTRICAL_PULSE_INFINITE = 0xE3
FORCE_POLL_REQ = 0x42
GET_DIAG_FLAGS = 0x80
SET_DIAG_FLAGS = 0x82
CLR_DIAG_FLAGS = 0x84

FLAG_FOTA_DISABLED = (1 << 0)
FLAG_CELLULAR_THREAD_DISABLED = (1 << 1)
FLAG_GNSS_THREAD_DISABLED = (1 << 2)
FLAG_BUZZER_DISABLED = (1 << 3)

GROUP_STORAGE = 0x03

GROUP_MODEM = 0x04
CMD_GET_CCID = 0x00
CMD_GET_VINT_STAT = 0x01
CMD_GET_IP = 0x02

RESP_ACK = 0x00
RESP_DATA = 0x01
RESP_CHK_FAILED = 0xC0
RESP_NOT_ENOUGH = 0xD0
RESP_NOT_IMPLEMENTED = 0xD1
RESP_ERROR = 0xE0
RESP_UNKNOWN_CMD = 0xFC
RESP_UNKNOWN_GRP = 0xFE
RESP_UNKNOWN = 0xFF

#gnss_struct_t = namedtuple('gnss_struct_t',['lat','lon','x','y','overflow','height','speed','head_veh','h_dop','h_acc_dm','v_acc_dm','head_acc','num_sv','pvt_flags','pvt_valid','updated_at','msss','ttff'])
gnss_struct_t = namedtuple('gnss_struct_t',['num_sv','pvt_flags','pvt_valid','overflow','x','y','height','speed','head_veh','h_dop','h_acc_dm','v_acc_dm','head_acc','lat','lon','updated_at','msss','ttff'])


class Commander(threading.Thread):
	def __init__(self, stream):
		threading.Thread.__init__(self)

		self.stream = stream
		crc16_conf = crc.Configuration(
			width=16,
			polynomial=0x1021,
			init_value=0x0000,
			final_xor_value=0x0000,
			reverse_input=True,
			reverse_output=True,
		)
		self.crc_calc = crc.CrcCalculator(crc16_conf)

		self.resp_queue = Queue()

		self.running = True
		self.daemon = True
		self.start()

	def __del__(self):
		self.stop()

	def stop(self):
		self.running = False
		self.join()

	def modem_get_ccid(self):
		resp = self.send_cmd(GROUP_MODEM, CMD_GET_CCID)
		if resp:
			if resp["code"] == RESP_DATA:
				value = struct.unpack("<" + str(len(resp["data"])) + "s", resp["data"])
				return value[0]
			else:
				return b""
		return None

	def modem_get_ip(self):
		resp = self.send_cmd(GROUP_MODEM, CMD_GET_IP)
		if resp:
			if resp["code"] == RESP_DATA:
				value = struct.unpack("<" + str(len(resp["data"])) + "s", resp["data"])
				return value[0]
			else:
				return b""
		return None

	def thread_control(self, value):
		print(f"{value=}")
		payload = struct.pack("<B", value)
		resp = self.send_cmd(GROUP_SYSTEM, CMD_THREAD_CONTROL, payload)
		return resp

	def force_poll_req(self):
		resp = self.send_cmd(GROUP_SYSTEM, CMD_FORCE_POLL_REQ)
		logging.debug(resp)
		return resp

	def electric_pulse_now(self):
		resp = self.send_cmd(GROUP_STIMULATOR, CMD_ELECTRICAL_PULSE)
		# if resp:
		# 	if resp["code"] == RESP_DATA:
		# 		value = struct.unpack("<" + str(len(resp["data"])) + "s", resp["data"])
		# 		return value[0]
		# 	else:
		# 		return b""
		logging.debug(resp)
		return resp

	def electric_pulse_infinite(self, value):
		payload = struct.pack("<B" + value)
		resp = self.send_cmd(GROUP_STIMULATOR, CMD_ELECTRICAL_PULSE_INFINITE, payload)
		
		logging.debug(resp)
		return resp		

	def start_buzzer(self):
		resp = self.send_cmd(GROUP_STIMULATOR, CMD_BUZZER_WARN)
		# if resp:
		# 	if resp["code"] == RESP_DATA:
		# 		value = struct.unpack("<" + str(len(resp["data"])) + "s", resp["data"])
		# 		return value[0]
		# 	else:
		# 		return b""
		logging.debug(resp)
		return resp

	def turn_onoff_charging(self):
		resp = self.send_cmd(GROUP_STIMULATOR, CMD_TURN_ONOFF_CHARGING)
		# if resp:
		# 	if resp["code"] == RESP_DATA:
		# 		value = struct.unpack("<" + str(len(resp["data"])) + "s", resp["data"])
		# 		return value[0]
		# 	else:
		# 		return b""
		logging.debug(resp)
		return resp

	def get_onboard_data(self):
		resp = self.send_cmd(GROUP_STIMULATOR, CMD_GET_ONBOARD_DATA)
		if resp:
			if resp["code"] == RESP_DATA:								
				return resp
			else:
				print("response failed")
				return b""
		return None

	def enter_sleep(self):
		resp = self.send_cmd(GROUP_SYSTEM, CMD_ENTER_SLEEP)
		print("ENTER SLEEP ->")
		logging.debug(resp)
		return resp

	def write_setting(self, id, value):
		if id[1] == "s":
			payload = struct.pack("<B" + str(len(value)) + id[1], id[0], value)
		else:
			payload = struct.pack("<B" + id[1], id[0], value)
		resp = self.send_cmd(GROUP_SETTINGS, CMD_WRITE, payload)
		logging.debug(resp)
		return resp
	
	def read_setting(self, id):
		payload = struct.pack("<B", id[0])
		resp = self.send_cmd(GROUP_SETTINGS, CMD_READ, payload)
		if resp:
			if id[1] == "s":
				_,value = struct.unpack("<B" + str(len(resp["data"])-1) + id[1], resp["data"])
			else:
				_,value = struct.unpack("<B" + id[1], resp["data"])
			return value
		return None

	def send_cmd(self, group, cmd, data=None, timeout=0.5):
		struct_format = "<BBH"
		raw_cmd = struct.pack(struct_format, group, cmd, 0)
		if not (data is None or len(data) == 0):
			raw_cmd += data

		checksum = self.crc_calc.calculate_checksum(raw_cmd)

		raw_cmd = bytearray(raw_cmd)

		raw_cmd[2] = checksum&0xFF
		raw_cmd[3] = (checksum>>8)&0xFF

		logging.debug("Sending: " + str(raw_cmd))

		self.stream.write(cobs.encode(raw_cmd) + b"\x00")

		return self.get_resp(group, cmd, timeout=timeout)

	def get_resp(self, group, cmd, timeout=0.5):
		resp = None
		end_time = time.time() + timeout
		while (not resp) and (time.time() < end_time):
			try:
				resp = self.resp_queue.get(timeout=end_time - time.time())

				if resp["group"] != group or resp["cmd"] != cmd:
					resp = None
			except:
				pass
		
		return resp

	def handle_resp(self, resp):
		logging.debug("Response: " + str(resp))

		resp_struct_format = "<BBBH"
		
		size = len(resp)
		if size < struct.Struct(resp_struct_format).size:
			logging.warning("Response too short")
			return
		
		data_size = size - struct.Struct(resp_struct_format).size
		data = None
		if data_size > 0:
			resp_struct_format += str(data_size) + "s"
			group, cmd, code, checksum, data = struct.unpack(resp_struct_format, resp)
		else:
			group, cmd, code, checksum = struct.unpack(resp_struct_format, resp)
		
		# Validate checksum
		resp = bytearray(resp)
		resp[3] = 0
		resp[4] = 0
		calc_checksum = self.crc_calc.calculate_checksum(resp)
		if checksum != calc_checksum:
			#logging.warning("Invalid checksum")
			return
		
		logging.debug("Got response: ")
		logging.debug("    group=" + str(group))
		logging.debug("    cmd=" + str(cmd))
		logging.debug("    code=" + str(code))
		logging.debug("    data=" + str(data))

		response = {}
		response["group"] = group
		response["cmd"] = cmd
		response["code"] = code
		response["data"] = data

		self.resp_queue.put(response)


	def run(self):
		receive_buffer = b""

		while self.running:
			try:
				data = self.stream.read(1000)

				if len(data) > 0:
					receive_buffer += data

					# Decode COBS format data
					# Parse as long as 0's are found 
					found_zero = True
					while found_zero:
						ind = receive_buffer.find(b"\x00")
						found_zero = (ind >= 0)
						if found_zero:
							# Identified a COBS encoded packet, fetch and remove from buffer
							logging.debug("Received: " + str(receive_buffer))
							enc = data[:ind]
							receive_buffer = receive_buffer[ind+1:]

							logging.debug("COBS-data: " + str(enc))
							resp = cobs.decode(enc)
							logging.debug("Decoded data: " + str(resp))

							self.handle_resp(resp)
				else:
					time.sleep(0.001)
			except Exception as e:
				time.sleep(0.001)

class TestStream:
	def __init__(self):
		self.read_queue = Queue()
	
	def inject_read_data(self, data):
		self.read_queue.put(data)

	def read(self, max_size):
		data = None
		try:
			data = self.read_queue.get(block=False)
		except Empty:
			pass
		return data
	
	def write(self, data):
		logging.debug("Writing: " + str(data))

if __name__ == "__main__":
	stream = TestStream()
	cmndr = Commander(stream)

	cmndr.send_cmd(1, 0)
	stream.inject_read_data(b'\x02\x01\x04\x13b\xb0\x00')

	while True:
		time.sleep(1)