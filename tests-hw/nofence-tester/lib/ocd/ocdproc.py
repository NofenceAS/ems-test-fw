import subprocess
import threading
import logging
import time

class OCDProc(threading.Thread):
    def __init__(self, config_file = None):
        threading.Thread.__init__(self)

        cmd = ['openocd']
        if config_file:
            cmd += ['-f', config_file]

        self.proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        self.running = True
        self.active = False

        self.daemon = True

        self.start()
    
    def __del__(self):
        self.close()

    def close(self):
        self.running = False
        self.proc.kill()
        self.join()

    def run(self):
        while self.running:
            line = self.proc.stderr.readline()
            if line != None:
                print(line)
                if b"Listening on port 4444 for telnet connections" in line:
                    self.active = True
            else:
                time.sleep(0.01)

    def is_active(self):
        return self.active