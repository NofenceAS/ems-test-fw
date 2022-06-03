from .commander import Commander
from .stream import BLEStream, JLinkStream

import time

RESP_ACK = 0

GRP_SYSTEM = 0x00,
GRP_SETTINGS = 0x01
GRP_STIMULATOR = 0x02
GRP_STORAGE = 0x03

CMD_BUZZER_WARN = 0xB0
CMD_ELECTRICAL_PULSE = 0xE0

class NFDiag:
	def __init__(self):
		self.stream = JLinkStream()
		#self.stream = BLEStream("COM4", serial=8010)
		while not self.stream.is_connected():
			time.sleep(0.1)

		self.cmndr = Commander(self.stream)
		print("Trying to ping...")
		start_time = time.time()
		while time.time() < (start_time + 5):
			resp = self.cmndr.send_cmd(0, 0x55)
			if resp:
				break
		
		self.cmndr.send_cmd(0x02, 0x10, b"\x03")

		while True:
			resp = self.cmndr.send_cmd(0x02, 0x12)
			if resp:
				if resp["data"]:
					print(resp["data"])
			else:
				time.sleep(0.01)

	def __del__(self):
		self.cmndr.stop()
		del self.cmndr
		del self.stream