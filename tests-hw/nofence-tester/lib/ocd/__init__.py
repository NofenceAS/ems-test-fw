from .ocdproc import OCDProc
from .rtt import RTT

from telnetlib import Telnet
import time
import logging
import os

class OCD:
    def __init__(self, board="X25"):
        self._board = board

        self._rtt = {}
        self._rtt_ch_up_cnt = 0
        self._rtt_ch_dn_cnt = 0

        base_path = os.path.dirname(os.path.abspath(__file__))
        if self._board == "X25":
            self._config_file = os.path.join(base_path, "openocd_nrf52840.cfg")
        else:
            raise Exception("Undefined board: " + self._board)
        self._o = OCDProc(self._config_file)

        start_time = time.time()
        while not self._o.is_active():
            time.sleep(0.1)

            if (time.time()-start_time) > 10.0:
                raise Exception("Timed out")

        self._tn = Telnet('localhost', 4444)

        self.wait_for_start()
    
    def __del__(self):
        for ch in self._rtt.keys():
            if "stream" in self._rtt[ch].keys():
                if self._rtt[ch]["stream"]:
                    self._rtt[ch].close()
        self._tn.close()
        self._o.close()
    
    def wait_for_start(self, timeout=10):
        waiting = True
        start = time.time()
        while waiting:
            try:
                data = self._tn.read_eager()
                if len(data) > 0:
                    if b"> " in data:
                        waiting = False
            except:
                time.sleep(0.01)
                
            if (time.time() - start) > timeout:
                raise Exception("Timed out")
        
    def send_cmd(self, cmd, timeout=10):
        # Check errors to clear
        self._o.got_error()

        # Send command
        logging.debug("Send CMD: \"" + cmd + "\"")
        self._tn.write((cmd + "\n").encode("utf-8"))

        # - Wait for echo and '> '
        resp = ""
        start = time.time()
        line = ""
        while (resp.find(cmd) == -1) or (not resp.endswith("> ")):
            try:
                data = self._tn.read_eager()
                if len(data) > 0:
                    new_data = data.decode("utf-8")
                    resp += new_data

                    # For logging purposes, find whole lines
                    while len(new_data) > 0:
                        newline = new_data.find("\n")
                        if newline == -1:
                            line += new_data
                            new_data = ""
                        else:
                            line += new_data[:newline+1]
                            new_data = new_data[newline+1:]

                            logging.debug(line.rstrip("\r\n"))
                            line = ""
            except:
                pass
            
            if (time.time() - start) > timeout:
                raise Exception("Timed out")
        
        result = resp[resp.find(cmd)+len(cmd):-2]

        got_error = self._o.got_error()

        return got_error, result

    def reset(self, halt=False, timeout=5):
        cmd = "reset "
        if halt:
            cmd += "halt"
        else:
            cmd += "run"
        error, result = self.send_cmd(cmd, timeout)
        logging.debug(result)

        if error:
            raise Exception("OpenOCD reported an error")
    
    def halt(self, timeout=2):
        cmd = "halt"
        error, result = self.send_cmd(cmd, timeout)
        logging.debug(result)

        if error:
            raise Exception("OpenOCD reported an error")
    
    def resume(self, timeout=2):
        cmd = "resume"
        error, result = self.send_cmd(cmd, timeout)
        logging.debug(result)

        if error:
            raise Exception("OpenOCD reported an error")
    
    def flash_write(self, image, timeout=30):
        cmd = "flash write_image erase " + image
        error, result = self.send_cmd(cmd, timeout)
        logging.debug(result)

        if error:
            raise Exception("OpenOCD reported an error")

    def flash_verify(self, image, timeout=30):
        cmd = "flash verify_image " + image
        error, result = self.send_cmd(cmd, timeout)
        logging.debug(result)

        if error:
            raise Exception("OpenOCD reported an error")
        
        verified = False
        for line in result.splitlines():
            if line.startswith("verified ") and " bytes from file " in line:
                verified = True
        if not verified:
            raise Exception("Response does not indicate successful verification of image")
    
    def _rtt_channel_parse(self, resp):
        expecting_up_ch = False
        expecting_dn_ch = False
        for line in resp.splitlines():
            if line.startswith("Channels: "):
                tmp = line.split(":")[1].split(",")

                parts = tmp[0].strip(" ").split("=")
                if not parts[0] == "up":
                    raise Exception("Unexpected value while getting channel count")
                self._rtt_ch_up_cnt = int(parts[1])

                parts = tmp[1].strip(" ").split("=")
                if not parts[0] == "down":
                    raise Exception("Unexpected value while getting channel count")
                self._rtt_ch_dn_cnt = int(parts[1])
            elif line.startswith("Up-channels:"):
                expecting_up_ch = True
                expecting_dn_ch = False
            elif line.startswith("Down-channels:"):
                expecting_up_ch = False
                expecting_dn_ch = True
            elif expecting_up_ch:
                tmp = line.split(":")
                if len(tmp) != 2:
                    continue

                ch = int(tmp[0])
                if not ch in self._rtt.keys():
                    self._rtt[ch] = {}

                parts = tmp[1].strip(" ").split(" ")
                self._rtt[ch]["up_name"] = parts[0]
                self._rtt[ch]["up_size"] = int(parts[1])
                self._rtt[ch]["up_flags"] = int(parts[2])
            elif expecting_dn_ch:
                tmp = line.split(":")
                if len(tmp) != 2:
                    continue

                ch = int(tmp[0])
                if not ch in self._rtt.keys():
                    self._rtt[ch] = {}

                parts = tmp[1].strip(" ").split(" ")
                self._rtt[ch]["dn_name"] = parts[0]
                self._rtt[ch]["dn_size"] = int(parts[1])
                self._rtt[ch]["dn_flags"] = int(parts[2])

    def rtt_prepare(self, timeout=10):
        cmd = "rtt setup 0x20000000 262144 \"SEGGER RTT\""
        error, result = self.send_cmd(cmd, timeout)
        logging.debug(result)
        if error:
            raise Exception("OpenOCD reported an error")
        
        cmd = "rtt start"
        error, result = self.send_cmd(cmd, timeout)
        logging.debug(result)
        if error:
            raise Exception("OpenOCD reported an error")
        
        cmd = "rtt channels"
        error, result = self.send_cmd(cmd, timeout)
        logging.debug(result)
        if error:
            raise Exception("OpenOCD reported an error")
        self._rtt_channel_parse(result)
        
        logging.debug(self._rtt)
    
    def _rtt_resolve_channel(self, ch_name):
        channel = None
        for ch in self._rtt.keys():
            if (self._rtt[ch]["dn_name"] == ch_name) or (self._rtt[ch]["up_name"] == ch_name):
                channel = ch
                break
        if not channel:
            raise Exception("Channel not found")
        return channel

    def rtt_connect(self, ch_name, port, timeout=10):
        channel = self._rtt_resolve_channel(ch_name)
        cmd = "rtt server start " + str(port) + " " + str(channel)
        error, result = self.send_cmd(cmd, timeout)
        logging.debug(result)

        if error:
            raise Exception("OpenOCD reported an error")

        self._rtt[channel]["stream"] = RTT(port)

        return self._rtt[channel]["stream"]