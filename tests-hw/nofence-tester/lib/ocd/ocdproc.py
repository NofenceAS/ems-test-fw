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

        self.lock = threading.Lock()

        self.running = True
        self.active = False

        self.daemon = True

        self.error = False

        self.start()
    
    def __del__(self):
        self.close()

    def close(self):
        self.running = False
        self.proc.kill()
        self.join()
    
    def got_error(self):
        self.lock.acquire()

        err = self.error
        self.error = False
        
        self.lock.release()
        
        return err

    def run(self):
        while self.running:
            self.lock.acquire()
            try:
                line = self.proc.stderr.readline()
                if line != None:
                    logging.debug(line.decode("utf-8"))
                    if b"Listening on port 4444 for telnet connections" in line:
                        self.active = True
                    if line.startswith(b"Error: "):
                        self.error = True
                else:
                    time.sleep(0.01)
            finally:
                self.lock.release()

    def is_active(self):
        return self.active