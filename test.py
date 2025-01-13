import serial
import threading
import sys
import time

ser = serial.Serial(sys.argv[1], 115200)

def read_serial():
    while True:
        r = ser.read()
        print(r.decode("utf-8"), end="")


t = threading.Thread(target=read_serial, daemon=True)
t.start()

for line in sys.stdin:
    ser.write(b"abcdefgh"*8)
