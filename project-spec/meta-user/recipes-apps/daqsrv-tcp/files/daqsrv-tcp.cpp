// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright 2025, University of Ljubljana
 *
 * This file is part of Cora-Z7-DAQ-OS.
 * Cora-Z7-DAQ-OS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or any later version.
 * Cora-Z7-DAQ-OS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with Cora-Z7-DAQ-OS.
 * If not, see <https://www.gnu.org/licenses/>.
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
	using boost::asio::ip::tcp;

	if (argc < 2) {
		std::cout << "Specify port for the server!" << std::endl;
		return -1;
	}

	int const port = std::stoi(std::string(argv[1]));

	try {
		boost::asio::io_service io_service;

		tcp::endpoint endpoint = tcp::endpoint(tcp::v4(), port);
		tcp::acceptor acceptor(io_service, endpoint);
		tcp::socket socket(io_service);

		while (true)
		{
			std::cout << "Listening on : " << endpoint << std::endl;

			acceptor.accept(socket);

			std::cout << socket.remote_endpoint() << " connected." << std::endl;

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
						boost::system::error_code err;
						auto sent = socket.send(boost::asio::buffer(buffer, BUFFER_SIZE), 0, err);
					
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
