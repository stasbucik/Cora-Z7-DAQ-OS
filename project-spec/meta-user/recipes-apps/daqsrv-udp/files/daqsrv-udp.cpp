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
#include <poll.h>

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
	boost::asio::io_context &io_context);

typedef std::function<void(
	boost::asio::ip::udp::socket &,
	boost::asio::ip::udp::endpoint &,
	boost::asio::io_context &)> onConnectSignature;

static bool connected = false;

void waitForConnection(boost::asio::ip::udp::socket &socket,
	boost::asio::ip::udp::endpoint &remote_endpoint,
	boost::asio::io_context &io_context,
	std::shared_ptr<onConnectSignature> completion_handler_ptr)
{
	std::shared_ptr<boost::array<uint8_t, 2>> recv_buf_ptr = std::make_shared<boost::array<uint8_t, 2>>();
	try {

		std::cout << "Listening on : " << socket.local_endpoint() << std::endl;

		socket.async_receive_from(boost::asio::buffer(*recv_buf_ptr), remote_endpoint, 0,
			[recv_buf_ptr, completion_handler_ptr, &socket, &remote_endpoint, &io_context]
			(const boost::system::error_code &err, std::size_t bytes_transferred)
			{
				if (err.failed()) {
					std::cout << "Error occured when reading from socket: " << err.to_string() << std::endl;
					return boost::asio::post(io_context,
						[&]()
						{
							waitForConnection(socket, remote_endpoint, io_context, std::make_shared<onConnectSignature>(onConnect));
						});
				}

				if (bytes_transferred != 2) {
					std::cout << "Didn't receive 2 bytes!" << std::endl;
					return boost::asio::post(io_context,
						[&]()
						{
							waitForConnection(socket, remote_endpoint, io_context, std::make_shared<onConnectSignature>(onConnect));
						});
				}

				if ((*recv_buf_ptr)[0] != PACKET_TYPE_CONNECT) {
					std::cout << "Received packet not connect packet." << std::endl;
					return boost::asio::post(io_context,
						[&]()
						{
							waitForConnection(socket, remote_endpoint, io_context, std::make_shared<onConnectSignature>(onConnect));
						});
				}

				std::string sample_rate = std::to_string(static_cast<uint32_t>((*recv_buf_ptr)[1]));

				int fd = open("/sys/kernel/daqdrv/sampleRate", O_WRONLY);
				if (fd == -1) {
					std::cout << "Error occured when opening /sys/kernel/daqdrv/sampleRate: " << errno << std::endl;
					return boost::asio::post(io_context,
						[&]()
						{
							waitForConnection(socket, remote_endpoint, io_context, std::make_shared<onConnectSignature>(onConnect));
						});
				}

				int ret_write = write(fd, sample_rate.c_str(), sample_rate.size());

				if (ret_write == -1) {
					close(fd);
					std::cout << "Error when writing to /sys/kernel/daqdrv/sampleRate: " << errno << std::endl;
					return boost::asio::post(io_context,
						[&]()
						{
							waitForConnection(socket, remote_endpoint, io_context, std::make_shared<onConnectSignature>(onConnect));
						});
				} else if (ret_write == 0) {
					close(fd);
					std::cout << "Error when writing to /sys/kernel/daqdrv/sampleRate: nothing was written." << std::endl;
					return boost::asio::post(io_context,
						[&]()
						{
							waitForConnection(socket, remote_endpoint, io_context, std::make_shared<onConnectSignature>(onConnect));
						});
				}

				close(fd);

				std::cout << remote_endpoint << " connected." << std::endl;
				connected = true;
				return (*completion_handler_ptr)(socket, remote_endpoint, io_context);

			});

	} catch (...) {
		std::cout << "Error!" << std::endl;
		std::cout << boost::current_exception_diagnostic_information() << std::endl;
		return;
	}
}

void checkDisconnect(boost::asio::ip::udp::socket &socket,
	boost::asio::ip::udp::endpoint &remote_endpoint,
	boost::asio::io_context &io_context)
{
	std::shared_ptr<boost::array<uint8_t, 1>> recv_buf_ptr = std::make_shared<boost::array<uint8_t, 1>>();
	try {

		socket.async_receive_from(boost::asio::buffer(*recv_buf_ptr), remote_endpoint, 0,
			[recv_buf_ptr, &socket, &remote_endpoint, &io_context]
			(const boost::system::error_code &err, std::size_t bytes_transferred)
			{
				if (err.failed()) {
					std::cout << "Error occured when reading from socket: " << err.to_string() << std::endl;
					return;
				}

				if (bytes_transferred != 1) {
					std::cout << "Did not receive packet of length 1." << std::endl;
					return;
				}

				if ((*recv_buf_ptr)[0] != PACKET_TYPE_DISCONNECT) {
					std::cout << "Received packet not disconnect packet." << std::endl;
					return;
				}

				std::cout << remote_endpoint << " disconnected." << std::endl;
				connected = false;
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
	boost::asio::io_context &io_context,
	int driver_fd,
	uint16_t packetCounter,
	std::shared_ptr<std::function<void(void)>> on_disconnect_handler_ptr,
	std::shared_ptr<std::function<void(void)>> on_error_handler_ptr)
{
	if (!connected) {
		return (*on_disconnect_handler_ptr)();
	}

	uint8_t packet_buffer[PACKET_SIZE];
	struct pollfd pfd;

	pfd.fd = driver_fd;
	pfd.events = POLLIN | POLLRDNORM;

	int poll_retval = poll(&pfd, 1, 1000);

	if (poll_retval < 0) {
		std::cout << "Error occured when polling /dev/daqdrv: " << errno << std::endl;
		return (*on_error_handler_ptr)();
	} else if (poll_retval == 0) {
		std::cout << "Polling /dev/daqdrv timed out." << std::endl;
		return (*on_error_handler_ptr)();
	}

	if ((pfd.revents & POLLIN) != POLLIN) {
		std::cout << "Polling /dev/daqdrv returned flags " << pfd.revents << std::endl;
		return (*on_error_handler_ptr)();
	}

	ssize_t read_retval = read(driver_fd, packet_buffer + PACKET_OFFSET_DATA, PACKET_SIZE_DATA);

	if (read_retval == -1) {
		std::cout << "Error occured when reading /dev/daqdrv: " << errno << std::endl;
		return (*on_error_handler_ptr)();
	} else if (read_retval == 0) {
		std::cout << "Reading /dev/daqdrv returned no data." << std::endl;
		return (*on_error_handler_ptr)();
	}

	try {
		uint8_t *pckt_type = (uint8_t *)((void *)(packet_buffer) + PACKET_OFFSET_TYPE);
		*pckt_type = PACKET_TYPE_DATA;

		uint16_t *pckt_counter = (uint16_t *)((void *)(packet_buffer) + PACKET_OFFSET_COUNTER);
		*pckt_counter = packetCounter;

		boost::system::error_code err;	
		socket.async_send_to(boost::asio::buffer(packet_buffer, PACKET_SIZE), remote_endpoint, 0,
			[&socket, &remote_endpoint, &io_context, driver_fd, packetCounter, on_disconnect_handler_ptr, on_error_handler_ptr]
			(const boost::system::error_code &err, std::size_t bytes_transferred)
			{
				if (err.failed()) {
					std::cout << "Error occured when writing to socket: " << err.to_string() << std::endl;
					return (*on_error_handler_ptr)();
				}

				if (bytes_transferred != PACKET_SIZE) {
					std::cout << "Didn't send full packet: " << bytes_transferred << std::endl;
					return (*on_error_handler_ptr)();
				}

				boost::asio::post(io_context,
					[&socket, &remote_endpoint, &io_context, driver_fd, packetCounter, on_disconnect_handler_ptr, on_error_handler_ptr]()
					{
						sendData(socket, remote_endpoint, io_context, driver_fd, packetCounter+1, on_disconnect_handler_ptr, on_error_handler_ptr);
					});
			});
	
	} catch (...) {
		std::cout << "Error on socket send" << std::endl;
		std::cout << boost::current_exception_diagnostic_information() << std::endl;
		return (*on_error_handler_ptr)();
	}
}

void onConnect(boost::asio::ip::udp::socket &socket,
	boost::asio::ip::udp::endpoint &remote_endpoint,
	boost::asio::io_context &io_context)
{
	int fd = open("/dev/daqdrv", O_RDONLY);
	if (fd == -1) {
		std::cout << "Error occured when opening /dev/daqdrv: " << errno << std::endl;
		return;
	}

	checkDisconnect(socket, remote_endpoint, io_context);

	sendData(socket, remote_endpoint, io_context, fd, 0, std::make_shared<std::function<void(void)>>(
		[fd, &socket, &remote_endpoint, &io_context]()
		{
			close(fd);
			boost::asio::post(io_context,
				[&]()
				{
					waitForConnection(socket, remote_endpoint, io_context, std::make_shared<onConnectSignature>(onConnect));
				});
		}),
		std::make_shared<std::function<void(void)>>(
		[fd]()
		{
			close(fd);
		}));
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

		waitForConnection(socket, remote_endpoint, io_context, std::make_shared<onConnectSignature>(onConnect));

		io_context.run();
		socket.close();

	} catch (...) {
		std::cout << "Error!" << std::endl;
		std::cout << boost::current_exception_diagnostic_information() << std::endl;
		return -1;
	}

	return 0;
}
