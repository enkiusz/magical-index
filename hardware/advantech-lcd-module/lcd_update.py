#!/usr/bin/env python3

import serial, sys

serial_port = sys.argv[1]
line1 = sys.argv[2]
line2 = sys.argv[3]

ser = serial.Serial(port=serial_port, baudrate=2400)

ser.write(bytes("{}{:<16}{}{:<16}".format("\x00" * 24, line1, "\x00" * 24, line2), 'ascii'))
