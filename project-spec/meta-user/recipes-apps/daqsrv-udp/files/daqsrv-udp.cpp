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
#include <string>

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/exception/diagnostic_information.hpp>

#define TIMEOUT 50
#define BUFFER_SIZE 256

int main(int argc, char *argv[])
{
	using boost::asio::ip::udp;

	if (argc < 2) {
		std::cout << "Specify port for the server!" << std::endl;
		return -1;
	}

	int const port = std::stoi(std::string(argv[1]));

	try {
		boost::asio::io_service io_service;

		udp::socket socket(io_service, udp::endpoint(udp::v4(), port));

		while (true)
		{
			boost::array<char, 1> recv_buf;
			udp::endpoint remote_endpoint;
			boost::system::error_code err;

			std::cout << "Listening on : " << socket.local_endpoint() << std::endl;

			socket.receive_from(boost::asio::buffer(recv_buf), remote_endpoint, 0, err);

			if (err.failed()) {
				std::cout << "Error occured when writing to socket: " << err.to_string() << std::endl;
				continue;
			}

			std::cout << remote_endpoint << " connected." << std::endl;

			char buffer[BUFFER_SIZE];
			int fd = open("/dev/daqdrv", O_RDONLY);
			if (fd == -1) {
				std::cout << "Error occured when opening /dev/daqdrv: " << errno << std::endl;
				return -1;
			}

			ssize_t dataRead = 0;
			int nullCount = 0;
			while (true) {
				dataRead = read(fd, buffer, BUFFER_SIZE);

				if (dataRead == -1) {
					if (errno == EAGAIN) {
						nullCount++;
			
						if (nullCount == TIMEOUT) {
							std::cout << TIMEOUT << " consecutive attempts to read failed, exiting." << std::endl;
							break;
						} else {
							std::this_thread::sleep_for(
								std::chrono::microseconds(200));
						}
					} else {
						std::cout << "Error occured when reading /dev/daqdrv: " << errno << std::endl;
						break;
					}
				} else if (dataRead > 0) {

					try {
						auto sent = socket.send_to(boost::asio::buffer(buffer, BUFFER_SIZE), remote_endpoint, 0, err);
					
						if (err.failed()) {
							std::cout << "Error occured when writing to socket: " << err.to_string() << std::endl;
							break;
						}

						if (sent != BUFFER_SIZE) {
							std::cout << "Didn't send full buffer: " << sent << std::endl;
							break;
						}
					} catch (...) {
						std::cout << "Error on socket send" << std::endl;
						std::cout << boost::current_exception_diagnostic_information() << std::endl;
						break;
					}

					nullCount = 0;
				}
			}

			close(fd);
			socket.close();
		}
	} catch (...) {
		std::cout << "Error!" << std::endl;
		std::cout << boost::current_exception_diagnostic_information() << std::endl;
		return -1;
	}

	return 0;
}
