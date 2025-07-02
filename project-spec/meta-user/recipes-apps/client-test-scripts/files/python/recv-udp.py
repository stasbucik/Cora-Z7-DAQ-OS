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
import sys

null_block = bytes([0x0 for j in range(0, 256)])

if len(sys.argv) < 4:
    print("Need sample_rate address and port as arguments.")
    print("recv-udp <sample_rate = [0-3]> <ip> <port>")
    exit(-1);
sample_rate = int(sys.argv[1])
srv_address = sys.argv[2]
port = int(sys.argv[3])

#srv_address = "cora-z7-daq-os.lan"

if (sample_rate < 0 or sample_rate > 3):
    print("Sample rate out of bounds [0-3].")
    exit(-1);

real_sample_rate = 1.0
if (sample_rate == 0):
    real_sample_rate = 2e5
elif (sample_rate == 1):
    real_sample_rate = 5e5
elif (sample_rate == 2):
    real_sample_rate = 1e6
elif (sample_rate == 3):
    real_sample_rate = 2e6

run = True

def inc(a):
    tmp = a + 1
    if tmp >= 2**16:
        tmp = tmp - 2**16
    return tmp

def diffPacketCntr(a, b):
    assert a < 2**16 and a >= 0, f'a ({a}) is not 16 bit number'
    assert b < 2**16 and b >= 0, f'b ({b})is not 16 bit number'
    tmp = a - b
    if tmp < 0:
        tmp = a + 2**16 - b
    assert tmp < 2**16 and b >= 0, f'result ({tmp})is not 16 bit number'
    return tmp


def handler(signal_received, frame):
    global run
    print('SIGINT or CTRL-C detected. Exiting gracefully')
    run = False

signal(SIGINT, handler)

client_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
client_socket.settimeout(3.0)
client_socket.sendto(b''.join([b'\x00', sample_rate.to_bytes(1, 'big')]), (srv_address, port))

alldataArray = []

try:
    packet_cntr = 0
    while run:
        data, server = client_socket.recvfrom(259)
        packet_type = data[0]
        recv_packet_cntr = (data[2] << 8) | data[1]
        
        if (recv_packet_cntr == packet_cntr):
            alldataArray.append(data[3:])
            packet_cntr = inc(packet_cntr)
        else:
            #print(f'expected {packet_cntr}, got {recv_packet_cntr}')
            alldataArray.append({'invalid': diffPacketCntr(recv_packet_cntr, packet_cntr)})
            alldata = bytes()

            packet_cntr = inc(recv_packet_cntr)

    client_socket.sendto(b'\x01', (srv_address, 44444))

except socket.timeout:
    print('REQUEST TIMED OUT')

try:
    client_socket.close()
except:
    print("socked didn't close properly")

samples = []

alldata = bytes()
for e in alldataArray:
    if type(e) == bytes:
        alldata = alldata + e
    elif type(e) == dict:
        for i in range(0, e['invalid']):
            alldata = alldata + null_block
    else:
        print(type(e))

for i in range(0, int(len(alldata)/4)):
    packet = alldata[4*i:4*i+4]
    sample1 = (packet[2] << 4) | ((packet[1] & 0xf0) >> 4)
    sample2 = ((packet[1] & 0xf) << 8) | packet[0]

    samples.append(sample1)
    samples.append(sample2)

sample_time = 1.0/real_sample_rate

time_samples = [i*sample_time for i in range(len(samples))]

plt.plot(time_samples, samples)
plt.title("Acquired samples")
plt.xlabel("Time / s")
plt.ylabel("Samples / LSB")
plt.show()
