import socket
import sys
import Queue
import threading

import time
import random

def ByteToHex( bins ):
    return ":".join("{:02x}".format(ord(c)) for c in bins)

q = Queue.PriorityQueue(0)

class ConsumerThread(threading.Thread):
    def run(self):
        while True:
            if not q.empty():
                item = q.get()
                if item[0] >= int(time.time()):
                    q.put(item)
                    continue;

                print ("echo bak", item[0], int(time.time()))
                item[1].sendto(item[2], item[3])
            time.sleep(0.001)

def listen():
    address = ('127.0.0.1', 12345)
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(address)
    while True:
        data , address = s.recvfrom(2048)
        print ("recv data",ByteToHex(data))
        second = random.randint(10, 20);
        print ("put", second )
        q.put((second +  int(time.time()), s, data, address, ))
        # time.sleep(1)

if __name__ == "__main__":
    try:

        thread1 = ConsumerThread()
        thread1.start()
        listen()
    except KeyboardInterrupt:
        thread1.stop()
        sys.exit(1)
        pass
