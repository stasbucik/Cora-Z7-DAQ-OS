/*
* Placeholder PetaLinux user application.
*
* Replace this with your application code

* Copyright (C) 2013-2022  Xilinx, Inc.  All rights reserved.
* Copyright (c) 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
*
* Permission is hereby granted, free of charge, to any person
* obtaining a copy of this software and associated documentation
* files (the "Software"), to deal in the Software without restriction,
* including without limitation the rights to use, copy, modify, merge,
* publish, distribute, sublicense, and/or sell copies of the Software,
* and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
* CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in this
* Software without prior written authorization from Xilinx.
*
*/

#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#define TIMEOUT 10
#define BUFFER_SIZE 4096

#define WRITE_SPEED 1000000.f
#define FPGA_BUFFER_SIZE 4096.f
#define WRITE_CYCLE FPGA_BUFFER_SIZE/WRITE_SPEED

char buffer[BUFFER_SIZE];

int main(int argc, char *argv[])
{
	int fd = open("/dev/daqdrv", O_RDONLY);
	if (fd == -1) {
		std::cout << "Error occured when opening /dev/daqdrv: " << errno << std::endl;
		return -1;
	}

	ssize_t dataRead = 0;
	ssize_t remaining = BUFFER_SIZE * 8;

	int nullCount = 0;
	while (remaining > 0) {
		dataRead = read(fd, buffer, BUFFER_SIZE);

		if (dataRead == -1) {
			if (errno == EAGAIN) {
				nullCount++;
	
				if (nullCount == TIMEOUT) {
					std::cout << TIMEOUT << " consecutive attempts to read failed, exiting." << std::endl;
					break;
				} else {
					std::this_thread::sleep_for(
						std::chrono::microseconds(static_cast<int>(std::round(WRITE_CYCLE*1e6/2.f))));
				}
			} else {
				std::cout << "Error occured when reading /dev/daqdrv: " << errno << std::endl;
				break;
			}
		} else if (dataRead > 0) {
			std::cout << dataRead << " bytes read." << std::endl;
			remaining -= dataRead;
			nullCount = 0;
		}
	}

	close(fd);
	return 0;
}


