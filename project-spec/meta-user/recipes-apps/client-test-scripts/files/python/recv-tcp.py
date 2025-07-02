#!/usr/bin/env python3

#  Copyright 2025, University of Ljubljana
#
#  This file is part of Cora-Z7-DAQ-OS.
#  Cora-Z7-DAQ-OS is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by the Free Software Foundation,
#  either version 3 of the License, or any later version.
#  Cora-Z7-DAQ-OS is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
#  FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#  You should have received a copy of the GNU General Public License along with Cora-Z7-DAQ-OS.
#  If not, see <https://www.gnu.org/licenses/>.

import socket
import matplotlib.pyplot as plt
from signal import signal, SIGINT

run = True

def handler(signal_received, frame):
    global run
    print('SIGINT or CTRL-C detected. Exiting gracefully')
    run = False

signal(SIGINT, handler)

client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client_socket.settimeout(3.0)
client_socket.connect(("192.168.1.10", 44444))

alldata = bytes()

try:
    while run:
        data = client_socket.recv(256)
        alldata = alldata + data
except socket.timeout:
    print('REQUEST TIMED OUT')

try:
    client_socket.close()
except:
    print("socked didn't close properly")

samples = []

for i in range(0, int(len(alldata)/4)):
    packet = alldata[4*i:4*i+4]
    sample1 = (packet[2] << 4) | ((packet[1] & 0xf0) >> 4)
    sample2 = ((packet[1] & 0xf) << 8) | packet[0]

    samples.append(sample1)
    samples.append(sample2)

plt.plot(samples)
plt.show()