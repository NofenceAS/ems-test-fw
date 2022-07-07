from cobs import cobs
import struct
from queue import Queue, Empty
import crc
import threading
import time

import logging
logging.basicConfig(level=logging.INFO)

# Definitions used for group/command/response
GROUP_SYSTEM = 0x00
CMD_PING = 0x55
CMD_LOG = 0x70
CMD_TEST = 0x7E

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
CMD_BUZZER_WARN = 0xB0
CMD_ELECTRICAL_PULSE = 0xE0

GROUP_STORAGE = 0x03

GROUP_MODEM = 0x04
CMD_GET_CCID = 0x00

RESP_ACK = 0x00
RESP_DATA = 0x01

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
			logging.warning("Invalid checksum")
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