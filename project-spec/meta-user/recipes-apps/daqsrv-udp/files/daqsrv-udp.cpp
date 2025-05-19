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
#include <functional>
#include <memory>

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/asio/post.hpp>

#define MAX_RETRY 50

#define PACKET_SIZE_TYPE sizeof(uint8_t)
#define PACKET_SIZE_COUNTER sizeof(uint16_t)
#define PACKET_SIZE_DATA 256
#define PACKET_SIZE PACKET_SIZE_TYPE + PACKET_SIZE_COUNTER + PACKET_SIZE_DATA

#define PACKET_OFFSET_TYPE 0
#define PACKET_OFFSET_COUNTER PACKET_OFFSET_TYPE + PACKET_SIZE_TYPE
#define PACKET_OFFSET_DATA PACKET_OFFSET_COUNTER + PACKET_SIZE_COUNTER

#define PACKET_TYPE_CONNECT 0
#define PACKET_TYPE_DISCONNECT 1
#define PACKET_TYPE_DATA 2

void onConnect(boost::asio::ip::udp::socket &socket,
	boost::asio::ip::udp::endpoint &remote_endpoint,
	boost::asio::io_context &io_context,
	boost::array<uint8_t, 1> &recv_buf);

typedef std::function<void(
	boost::asio::ip::udp::socket &,
	boost::asio::ip::udp::endpoint &,
	boost::asio::io_context &,
	boost::array<uint8_t, 1> &)> onConnectSignature;

static bool connected = false;

void waitForConnection(boost::asio::ip::udp::socket &socket,
	boost::asio::ip::udp::endpoint &remote_endpoint,
	boost::asio::io_context &io_context,
	boost::array<uint8_t, 1> &recv_buf,
	std::shared_ptr<onConnectSignature> completion_handler_ptr)
{
	try {

		std::cout << "Listening on : " << socket.local_endpoint() << std::endl;

		socket.async_receive_from(boost::asio::buffer(recv_buf), remote_endpoint, 0,
			[&recv_buf, completion_handler_ptr, &socket, &remote_endpoint, &io_context]
			(const boost::system::error_code &err, std::size_t bytes_transferred)
			{
				if (err.failed()) {
					std::cout << "Error occured when reading from socket: " << err.to_string() << std::endl;
					return boost::asio::post(io_context,
						[&]()
						{
							waitForConnection(socket, remote_endpoint, io_context, recv_buf, std::make_shared<onConnectSignature>(onConnect));
						});
				}

				if (bytes_transferred != 1) {
					std::cout << "Didn't receive 1 byte!" << std::endl;
					return boost::asio::post(io_context,
						[&]()
						{
							waitForConnection(socket, remote_endpoint, io_context, recv_buf, std::make_shared<onConnectSignature>(onConnect));
						});
				}

				if (recv_buf[0] != PACKET_TYPE_CONNECT) {
					std::cout << "Received packet not connect packet." << std::endl;
					return boost::asio::post(io_context,
						[&]()
						{
							waitForConnection(socket, remote_endpoint, io_context, recv_buf, std::make_shared<onConnectSignature>(onConnect));
						});
				}

				std::cout << remote_endpoint << " connected." << std::endl;
				connected = true;
				return (*completion_handler_ptr)(socket, remote_endpoint, io_context, recv_buf);

			});

	} catch (...) {
		std::cout << "Error!" << std::endl;
		std::cout << boost::current_exception_diagnostic_information() << std::endl;
		return;
	}
}

void checkDisconnect(boost::asio::ip::udp::socket &socket,
	boost::asio::ip::udp::endpoint &remote_endpoint,
	boost::asio::io_context &io_context,
	boost::array<uint8_t, 1> &recv_buf,
	std::shared_ptr<std::function<void(void)>> completion_handler_ptr)
{
	try {

		socket.async_receive_from(boost::asio::buffer(recv_buf), remote_endpoint, 0,
			[&recv_buf, &socket, &remote_endpoint, completion_handler_ptr, &io_context]
			(const boost::system::error_code &err, std::size_t bytes_transferred)
			{
				if (err.failed()) {
					std::cout << "Error occured when reading from socket: " << err.to_string() << std::endl;
					return boost::asio::post(io_context,
						[&]()
						{
							waitForConnection(socket, remote_endpoint, io_context, recv_buf, std::make_shared<onConnectSignature>(onConnect));
						});
				}

				if (bytes_transferred != 1) {
					std::cout << "Did not receive packet of length 1." << std::endl;
					return;
				}

				if (recv_buf[0] != PACKET_TYPE_DISCONNECT) {
					std::cout << "Received packet not disconnect packet." << std::endl;
					return;
				}

				std::cout << remote_endpoint << " disconnected." << std::endl;
				connected = false;
				return (*completion_handler_ptr)();
			});


	} catch (...) {
		std::cout << "Error!" << std::endl;
		std::cout << boost::current_exception_diagnostic_information() << std::endl;
		return;
	}
}

void sendData(
	boost::asio::ip::udp::socket &socket,
	boost::asio::ip::udp::endpoint &remote_endpoint,
	int driver_fd,
	uint16_t packetCounter)
{
	if (!connected) {
		return;
	}

	uint8_t packet_buffer[PACKET_SIZE];
	ssize_t dataRead = 0;
	uint32_t retryCount = 0;

	while (true) {

		dataRead = read(driver_fd, packet_buffer + PACKET_OFFSET_DATA, PACKET_SIZE_DATA);

		if (dataRead == -1) {
			if (errno != EAGAIN) {
				std::cout << "Error occured when reading /dev/daqdrv: " << errno << std::endl;
				return;
			} else {
				retryCount++;
	
				if (retryCount == MAX_RETRY) {
					std::cout << MAX_RETRY << " consecutive attempts to read failed, exiting." << std::endl;
					return;
				} else {
					std::this_thread::sleep_for(
						std::chrono::microseconds(200));
				}
			}
		} else if (dataRead > 0) {
			break;
		}
	}

	try {
		uint8_t *pckt_type = (uint8_t *)((void *)(packet_buffer) + PACKET_OFFSET_TYPE);
		*pckt_type = PACKET_TYPE_DATA;

		uint16_t *pckt_counter = (uint16_t *)((void *)(packet_buffer) + PACKET_OFFSET_COUNTER);
		*pckt_counter = packetCounter;

		boost::system::error_code err;	
		socket.async_send_to(boost::asio::buffer(packet_buffer, PACKET_SIZE), remote_endpoint, 0,
			[&socket, &remote_endpoint, driver_fd, packetCounter]
			(const boost::system::error_code &err, std::size_t bytes_transferred)
			{
				if (err.failed()) {
					std::cout << "Error occured when writing to socket: " << err.to_string() << std::endl;
					return;
				}

				if (bytes_transferred != PACKET_SIZE) {
					std::cout << "Didn't send full packet: " << bytes_transferred << std::endl;
					return;
				}

				sendData(socket, remote_endpoint, driver_fd, packetCounter+1);
			});
	
	} catch (...) {
		std::cout << "Error on socket send" << std::endl;
		std::cout << boost::current_exception_diagnostic_information() << std::endl;
		return;
	}
}

void onConnect(boost::asio::ip::udp::socket &socket,
	boost::asio::ip::udp::endpoint &remote_endpoint,
	boost::asio::io_context &io_context,
	boost::array<uint8_t, 1> &recv_buf)
{
	int fd = open("/dev/daqdrv", O_RDONLY);
	if (fd == -1) {
		std::cout << "Error occured when opening /dev/daqdrv: " << errno << std::endl;
		return;
	}

	checkDisconnect(socket, remote_endpoint, io_context, recv_buf, std::make_shared<std::function<void(void)>>(
		[fd, &socket, &remote_endpoint, &recv_buf, &io_context]()
		{
			close(fd);
			boost::asio::post(io_context,
				[&]()
				{
					waitForConnection(socket, remote_endpoint, io_context, recv_buf, std::make_shared<onConnectSignature>(onConnect));
				});
		}));

	sendData(socket, remote_endpoint, fd, 0);
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

		boost::array<uint8_t, 1> recv_buf;


		waitForConnection(socket, remote_endpoint, io_context, recv_buf, std::make_shared<onConnectSignature>(onConnect));

		io_context.run();
		socket.close();

	} catch (...) {
		std::cout << "Error!" << std::endl;
		std::cout << boost::current_exception_diagnostic_information() << std::endl;
		return -1;
	}

	return 0;
}
