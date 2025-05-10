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
#include <string>
#include <cstdint>

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/exception/diagnostic_information.hpp>

#define MAX_RETRY 50

#define PACKET_SIZE_TYPE sizeof(uint8_t)
#define PACKET_SIZE_COUNTER sizeof(uint16_t)
#define PACKET_SIZE_DATA 256
#define PACKET_SIZE PACKET_SIZE_TYPE + PACKET_SIZE_COUNTER + PACKET_SIZE_DATA

#define PACKET_OFFSET_TYPE 0
#define PACKET_OFFSET_COUNTER PACKET_OFFSET_TYPE + PACKET_SIZE_TYPE
#define PACKET_OFFSET_DATA PACKET_OFFSET_COUNTER + PACKET_SIZE_COUNTER

#define PACKET_TYPE_CONNECT 0
#define PACKET_TYPE_DATA 1

int waitForConnection(boost::asio::ip::udp::socket &socket,
	boost::asio::ip::udp::endpoint &remote_endpoint)
{
	try {

		boost::array<char, 1> recv_buf;
		boost::system::error_code err;

		std::cout << "Listening on : " << socket.local_endpoint() << std::endl;

		socket.receive_from(boost::asio::buffer(recv_buf), remote_endpoint, 0, err);

		if (err.failed()) {
			std::cout << "Error occured when reading from socket: " << err.to_string() << std::endl;
			return -2;
		}

		if (recv_buf[0] != PACKET_TYPE_CONNECT) {
			std::cout << "Received packet not connect packet." << std::endl;
			return -2;
		}

		std::cout << remote_endpoint << " connected." << std::endl;

	} catch (...) {
		std::cout << "Error!" << std::endl;
		std::cout << boost::current_exception_diagnostic_information() << std::endl;
		return -1;
	}

	return 0;
}

int waitForAck(boost::asio::ip::udp::socket &socket,
	boost::asio::ip::udp::endpoint &remote_endpoint,
	boost::asio::io_context &io_context)
{
	try {
		boost::array<char, 1> recv_buf;
		boost::system::error_code err;
		std::size_t length = 0;

		socket.async_receive_from(boost::asio::buffer(recv_buf), remote_endpoint, 0,
			[&err, &length](const boost::system::error_code &error,
				std::size_t bytes_transferred)
			{
				err = error;
				length = bytes_transferred;
			});

		io_context.restart();
		io_context.run_for(std::chrono::milliseconds(1000));
		if (!io_context.stopped()) {
			socket.cancel();
			std::cout << "Timeout reached when waiting for ACK. Dropping client." << std::endl;
			return -1;
		}

		if (err.failed()) {
			std::cout << "Error occured when reading from socket: " << err.to_string() << std::endl;
			return -1;
		}

		if (length != 1) {
			std::cout << "Did not receive packet of length 1." << std::endl;
			return -1;
		}

		//if (recv_buf[0] != PACKET_TYPE_ACK) {
		//	std::cout << "Received packet not ACK packet." << std::endl;
		//	return -1;
		//}
	} catch (...) {
		std::cout << "Error!" << std::endl;
		std::cout << boost::current_exception_diagnostic_information() << std::endl;
		return -1;
	}

	return 0;
}

void sendLoop(boost::asio::ip::udp::socket &socket,
	boost::asio::ip::udp::endpoint &remote_endpoint,
	int driver_fd)
{
	char packet_buffer[PACKET_SIZE];
	ssize_t dataRead = 0;
	uint32_t retryCount = 0;
	uint16_t packetCounter = 0;

	while (true) {
		dataRead = read(driver_fd, packet_buffer + PACKET_OFFSET_DATA, PACKET_SIZE_DATA);

		if (dataRead == -1) {
			if (errno != EAGAIN) {
				std::cout << "Error occured when reading /dev/daqdrv: " << errno << std::endl;
				break;
			} else {
				retryCount++;
	
				if (retryCount == MAX_RETRY) {
					std::cout << MAX_RETRY << " consecutive attempts to read failed, exiting." << std::endl;
					break;
				} else {
					std::this_thread::sleep_for(
						std::chrono::microseconds(200));
				}
			}
		} else if (dataRead > 0) {

			try {
				uint8_t *pckt_type = (uint8_t *)((void *)(packet_buffer) + PACKET_OFFSET_TYPE);
				*pckt_type = PACKET_TYPE_DATA;

				uint16_t *pckt_counter = (uint16_t *)((void *)(packet_buffer) + PACKET_OFFSET_COUNTER);
				*pckt_counter = packetCounter;

				boost::system::error_code err;	
				auto sent = socket.send_to(boost::asio::buffer(packet_buffer, PACKET_SIZE), remote_endpoint, 0, err);
			
				if (err.failed()) {
					std::cout << "Error occured when writing to socket: " << err.to_string() << std::endl;
					break;
				}

				if (sent != PACKET_SIZE) {
					std::cout << "Didn't send full packet: " << sent << std::endl;
					break;
				}

				packetCounter++;
			} catch (...) {
				std::cout << "Error on socket send" << std::endl;
				std::cout << boost::current_exception_diagnostic_information() << std::endl;
				break;
			}

			retryCount = 0;
		}
	}
}

int main(int argc, char *argv[])
{
	using boost::asio::ip::udp;

	if (argc < 2) {
		std::cout << "Specify port for the server!" << std::endl;
		return -1;
	}

	int const port = std::stoi(std::string(argv[1]));

	try {
		boost::asio::io_context io_context;

		udp::socket socket(io_context, udp::endpoint(udp::v4(), port));
		udp::endpoint remote_endpoint;

		while (true)
		{
			int ret_wait_connect = waitForConnection(socket, remote_endpoint);

			if (ret_wait_connect == -1) {
				break;
			} else if (ret_wait_connect == -2) {
				continue;
			}

			int fd = open("/dev/daqdrv", O_RDONLY);
			if (fd == -1) {
				std::cout << "Error occured when opening /dev/daqdrv: " << errno << std::endl;
				break;
			}

			sendLoop(socket, remote_endpoint, fd);

			close(fd);
		}

	socket.close();

	} catch (...) {
		std::cout << "Error!" << std::endl;
		std::cout << boost::current_exception_diagnostic_information() << std::endl;
		return -1;
	}

	return 0;
}
