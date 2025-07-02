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
#include <vector>
#include <array>
#include <string>
#include <cstdint>
#include <memory>
#include <chrono>
#include <thread>
#include <map>
#include <csignal>
#include <cassert>

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/exception/diagnostic_information.hpp>

#include "matplotlibcpp.h"

#define PACKET_DATA_LENGTH 256
#define PACKET_CONVERSION_LENGTH PACKET_DATA_LENGTH*3/4

namespace {
	std::shared_ptr<boost::asio::ip::udp::socket> socket_ptr = nullptr;
	std::shared_ptr<std::vector<uint8_t>> data_ptr = nullptr;
	std::shared_ptr<std::map<uint64_t, uint16_t>> invalid_ptr = nullptr;
	std::shared_ptr<boost::asio::io_context> iocontext_ptr = nullptr;
	std::shared_ptr<boost::array<uint8_t, 259>> recvbuf_ptr = nullptr;
	std::shared_ptr<bool> run_ptr;
	std::shared_ptr<double> real_sample_rate_ptr;
}

void showData()
{
	std::vector<uint16_t> samples;
	std::vector<double> time_samples;

	std::array<uint16_t, PACKET_CONVERSION_LENGTH> null_samples;
	null_samples.fill(0);

	double const sample_time = 1.0/(*real_sample_rate_ptr);
	uint64_t cntr = 0;

	auto start = std::begin(*data_ptr);

	for (auto k : *invalid_ptr)
	{
		std::cout << "At " << k.first << " lost " << k.second << std::endl;
		auto stop = std::begin(*data_ptr) + k.first;

		for(; start != stop; start += 4)
		{
			uint16_t sample1 = (*(start+2) << 4) | ((*(start+1) & 0xf0) >> 4);
			uint16_t sample2 = ((*(start+1) & 0xf) << 8) | *start;
			samples.insert(std::end(samples), sample1);
			samples.insert(std::end(samples), sample2);
			time_samples.insert(std::end(time_samples), static_cast<double>(cntr) * sample_time);
			time_samples.insert(std::end(time_samples), static_cast<double>(cntr+1) * sample_time);
			cntr += 2;
		}

		for (int i = 0; i < k.second; i++)
		{
			samples.insert(std::end(samples), std::begin(null_samples), std::end(null_samples));
		}

		for (uint64_t i = 0; i < k.second * PACKET_CONVERSION_LENGTH; i++)
		{
			time_samples.insert(std::end(time_samples), static_cast<double>(cntr + i) * sample_time);
		}
		cntr += k.second * PACKET_CONVERSION_LENGTH;
			
		//start = stop;
	}

	for(auto stop = std::end(*data_ptr); start != stop; start += 4)
	{
		uint16_t sample1 = (*(start+2) << 4) | ((*(start+1) & 0xf0) >> 4);
		uint16_t sample2 = ((*(start+1) & 0xf) << 8) | *start;
		samples.insert(std::end(samples), sample1);
		samples.insert(std::end(samples), sample2);
		time_samples.insert(std::end(time_samples), static_cast<double>(cntr) * sample_time);
		time_samples.insert(std::end(time_samples), static_cast<double>(cntr+1) * sample_time);
		cntr += 2;
	}

	if (samples.size() != time_samples.size()) {
		std::cout << "Samples do not match time!" << std::endl;
		return;
	}

	matplotlibcpp::figure();
	matplotlibcpp::plot(time_samples, samples);
	matplotlibcpp::title("Acquired samples");
	matplotlibcpp::xlabel("Time / s");
	matplotlibcpp::ylabel("Samples / LSB");
	matplotlibcpp::show();
	matplotlibcpp::detail::_interpreter::kill();
}

void recvData(uint16_t packet_cntr)
{
	try {
		auto timer = std::make_shared<boost::asio::deadline_timer>(*iocontext_ptr);

		auto recv_cpltn_hndlr = std::make_shared<std::function<void(void)>>(
			[packet_cntr, timer]()
			{
				timer->cancel();
				uint8_t packet_type = recvbuf_ptr->at(0);
    			uint16_t recv_packet_cntr = (recvbuf_ptr->at(2) << 8) | recvbuf_ptr->at(1);

    			uint16_t new_packet_cntr = 0;
    			if (packet_cntr != recv_packet_cntr) {

    				uint16_t packet_diff = 0;
    				if (packet_cntr > recv_packet_cntr) {
    					packet_diff = packet_cntr - recv_packet_cntr - 1;
    					packet_diff = 0xffff - packet_diff;
    				} else {
    					packet_diff = recv_packet_cntr - packet_cntr;
    				}
    				invalid_ptr->emplace(std::make_pair(data_ptr->size(), packet_diff));

    				if (recv_packet_cntr != 0xffff) {
	    				new_packet_cntr = recv_packet_cntr + 1;
	    			}
    			} else if (packet_cntr != 0xffff) {
    				new_packet_cntr = packet_cntr + 1;
    			}

    			data_ptr->insert(std::end(*data_ptr), std::begin(*recvbuf_ptr) + 3, std::end(*recvbuf_ptr));

    			if (*run_ptr) {
					boost::asio::post(*iocontext_ptr,
						[new_packet_cntr]()
						{
							recvData(new_packet_cntr);
						});
				} else {
			    	boost::asio::post(*iocontext_ptr,
						[]()
						{
							socket_ptr->async_send(boost::asio::buffer("\x01", 1), 0,
								[](const boost::system::error_code &err, std::size_t bytes_transferred)
								{
									if (err.failed()) {
										std::cout << "Error occured when disconnecting: " << err.to_string() << std::endl;
									}

									if (bytes_transferred != 1) {
										std::cout << "Didn't send full packet: " << bytes_transferred << std::endl;
									}
									socket_ptr->close();
									showData();
								});
						});
				}
			});

		socket_ptr->async_receive(boost::asio::buffer(*recvbuf_ptr), 0,
			[recv_cpltn_hndlr](const boost::system::error_code &err, std::size_t bytes_transferred)
			{
				if (err.failed()) {
					if (err.value() != ECANCELED) {
						std::cout << "Error occured when reading from socket: " << err.to_string() << std::endl;
					}
					return;
				}

				if (bytes_transferred != 259) {
					std::cout << "Didn't receive full packet: " << bytes_transferred << std::endl;
					return;
				}

		        if (recv_cpltn_hndlr != nullptr) {
		        	(*recv_cpltn_hndlr)();
		        } else {
		        	std::cout << "Read callback destroyed." << std::endl;
		        }
			});

		timer->expires_from_now(boost::posix_time::milliseconds(2000));
		timer->async_wait(
			[recv_cpltn_hndlr]
			(const boost::system::error_code &err) mutable
			{
				if (err.failed()) {
					return;
				}
				recv_cpltn_hndlr.reset();
				std::cout << "REQUEST TIMED OUT" << std::endl;
				boost::asio::post(*iocontext_ptr,
					[]()
					{
						socket_ptr->close();
						showData();
					});
			});
	} catch (...) {
		std::cout << "Error in recvData!" << std::endl;
		std::cout << boost::current_exception_diagnostic_information() << std::endl;
	}
}

void sigint_handler(int signal)
{
    if(signal == SIGINT) {
    	*run_ptr = false;
    } else {
    	std::cout << "signal is " << signal << std::endl;
    }
}

int main(int argc, char *argv[])
{
	using boost::asio::ip::udp;

	if (argc < 4) {
		std::cout << "Need sample_rate address and port as arguments." << std::endl;
		std::cout << "recv-udp <sample_rate = [0-3]> <ip> <port>" << std::endl;
		return -1;
	}

	std::signal(SIGINT, sigint_handler);

	try {
		run_ptr = std::make_shared<bool>(true);

		int sample_rate = std::stoi(std::string(argv[1]));
		if (sample_rate < 0 || sample_rate > 3) {
			std::cout << "Sample rate out of bounds [0-3]." << std::endl;
			return -1;
		}

		real_sample_rate_ptr = std::make_shared<double>(1.0);
		if (sample_rate == 0) {
			*real_sample_rate_ptr = 2e5;
		} else if (sample_rate == 1) {
			*real_sample_rate_ptr = 5e5;
		} else if (sample_rate == 2) {
			*real_sample_rate_ptr = 1e6;
		} else if (sample_rate == 3) {
			*real_sample_rate_ptr = 2e6;
		}

		iocontext_ptr = std::make_shared<boost::asio::io_context>();
		socket_ptr = std::make_shared<udp::socket>(*iocontext_ptr);

		udp::endpoint server_endpoint(
			boost::asio::ip::make_address(std::string(argv[2])),
			std::stoi(std::string(argv[3])));

		data_ptr = std::make_shared<std::vector<uint8_t>>();
		invalid_ptr = std::make_shared<std::map<uint64_t, uint16_t>>();
		recvbuf_ptr = std::make_shared<boost::array<uint8_t, 259>>();

		socket_ptr->connect(server_endpoint);
		
		boost::asio::socket_base::receive_buffer_size buff_size_option(0x8000000); // 128MiB
		boost::system::error_code er;
		socket_ptr->set_option(buff_size_option, er);
		if (er.failed()) {
			std::cout << er << std::endl;
			std::cout << "Unable to set the receive buffer to 128MiB." << std::endl;

			buff_size_option = 0;
			socket_ptr->set_option(buff_size_option, er);

			if (er.failed()) {
				std::cout << er << std::endl;
				std::cout << "Unable to get the receive buffer size." << std::endl;
			} else {
				std::cout << "Buffer size: " << buff_size_option.value() << std::endl;
			}
			std::cout << "Might experience packet loss." << std::endl;
		}

		uint8_t send_buffer[2] = { 0, static_cast<uint8_t>(sample_rate) };

		socket_ptr->async_send(boost::asio::buffer(send_buffer, 2), 0,
			[]
			(const boost::system::error_code &err, std::size_t bytes_transferred)
			{
				if (err.failed()) {
					std::cout << "Error occured when connecting: " << err.to_string() << std::endl;
					return;
				}

				if (bytes_transferred != 2) {
					std::cout << "Didn't send full packet: " << bytes_transferred << std::endl;
					return;
				}

				boost::asio::post(*iocontext_ptr,
					[]()
					{
						recvData(0);
					});
			});

		iocontext_ptr->run();

	} catch (...) {
		std::cout << "Error in main!" << std::endl;
		std::cout << boost::current_exception_diagnostic_information() << std::endl;
		return -1;
	}
}