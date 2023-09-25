import logging
import time
import sys
import socket
import threading
import chipwhisperer as cw
from esptool import main

class Usart2Socket(object):
    def __init__(self, usart, tcpport):
        self.usart = usart
        self.tcpport = tcpport
        self._write_lock = threading.Lock()
        self._sem = threading.Semaphore(2)
        self.alive = False
        self.connected = False

    def run(self):
        self.alive = True

        self.srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.srv.bind(('127.0.0.1', self.tcpport))
        self.srv.listen(1)
        self.srv.settimeout(0.1)
        self.thread_usart2socket = threading.Thread(target=self.usart2socket)
        self.thread_usart2socket.daemon = True
        self.thread_usart2socket.name = 'usart->socket'
        self.thread_usart2socket.start()
        self.thread_socket2usart = threading.Thread(target=self.socket2usart)
        self.thread_socket2usart.daemon = True
        self.thread_socket2usart.name = 'socket->usart'
        self.thread_socket2usart.start()

    def usart2socket(self):
        self._sem.acquire()
        try:
            while self.alive:
                try:
                    waiting = self.usart.inWaiting()
                    if waiting > 0:
                        data = self.usart.read(waiting, timeout = 1000)
                        #print("u2s: {}".format(data))
                        if self.connected:
                            with self._write_lock:
                                self.socket.sendall(data)
                    else:
                        time.sleep(0.01)
                except socket.timeout:
                    pass
                except socket.error as msg:
                    self.log.error('{}'.format(msg))
                    break
        finally:
            self.alive = False
            self.connected = False
            self._sem.release()

    def socket2usart(self):
        self._sem.acquire()
        try:
            while self.alive:
                try:
                    self.socket, addr = self.srv.accept()
                    self.socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                    self.socket.settimeout(0.01)
                    self.connected = True
                    break
                except socket.timeout:
                    pass

            while self.alive:
                try:
                    data = self.socket.recv(1024)
                    if not data:
                        break
                    #print("s2u: {}".format(data))
                    self.usart.write(data)
                except socket.timeout:
                    pass
                except socket.error as msg:
                    print('{}'.format(msg))
                    break
        finally:
            self.alive = False
            self.connected = False
            self._sem.release()

    def stop(self):
        if self.alive:
            self.alive = False
            # acquire the semaphore. This will only succeed when both threads have stopped
            self._sem.acquire()
            self._sem.release()
            if self.connected:
                self.connected = False
                self.socket.close()
            self.thread_usart2socket.join()
            self.thread_socket2usart.join()


class Esp32Programmer:
    """
    Class for programming an ESP32 processor
    """

    def __init__(self, scope, timeout=200):
        """
        Set the communications instance.
        """

        self.scope = scope
        self._cwserial = scope._get_usart()
        self._timeout = timeout
        self._esptool_lock = threading.Lock()

    def esptool(self, args):
        with self._esptool_lock:
            # TODO: Baudrate should be set to 115200
            # TODO: scope.clock should be set to 26E6
            proxy = Usart2Socket(self._cwserial, 2218)
            proxy.run()

            old_args = sys.argv         # Backup the original sys.argv array
            sys.argv = ['esptool', '--chip', 'esp32', '--port', 'socket://127.0.0.1:2218', '--before', 'no_reset', '--after', 'no_reset' ]
            sys.argv.extend(args)

            try:
                main()
            finally:
                proxy.stop()
                sys.argv = old_args     # Restore the original sys.argv array

    def find(self):
        return

    def program(self, romfilename, bsfile=None, check_rom_size=True):
        """Programs memory type, dealing with opening filename as either .hex or .bin file"""

        self.esptool([ 'write_flash', '-z', '--flash_mode', 'dio', '--flash_freq', '40m', '--flash_size', 'detect', '0x1000', romfilename ])

        # TODO: Size must fit between 0x1000 and 0x8000 --> max 0x7000
        #if check_rom_size:
        #    if len(romdata) > 0x7000:
        #        raise ValueError("This function uploads an ESP32 bootloader, size is limited to 28KB." +
        #                          " Binary appears to be too large, got %d bytes"%len(romdata))

# This file can be used stand-alone. It allows to run any esptool.py function through the CW.
if __name__ == '__main__':
    scope = cw.scope()

    scope.default_setup()
    scope.io.tio3 = False
    scope.io.pdic = False
    scope.clock.clkgen_freq = 26E6

    scope.io.nrst = False
    time.sleep(0.05)
    scope.io.nrst = None
    time.sleep(0.05)

    esp32 = Esp32Programmer(scope)
    esp32.esptool(sys.argv[1:])

    scope.io.tio3 = True
    scope.io.nrst = False
    time.sleep(0.05)
    scope.io.nrst = None
    time.sleep(0.05)
