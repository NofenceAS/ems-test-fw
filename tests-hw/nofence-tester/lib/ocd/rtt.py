import socket
import fcntl, os
import threading
import logging
import time

HOST = "127.0.0.1"

class RTT(threading.Thread):
    def __init__(self, port):
        threading.Thread.__init__(self)

        self.port = port

        self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.s.connect((HOST, self.port))
        fcntl.fcntl(self.s, fcntl.F_SETFL, os.O_NONBLOCK)
        #s.sendall(b"Hello, world")

        self.running = True

        self.start()

    def __del__(self):
        self.close()

    def close(self):
        self.running = False
        self.join()
        self.s.close()

    def run(self):
        while self.running:
            try:
                data = self.s.recv(1024)
                if len(data) > 0:
                    logging.debug(data.decode("utf-8"))
            except Exception as e:
                time.sleep(0.01)