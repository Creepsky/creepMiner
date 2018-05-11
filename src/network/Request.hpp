// ==========================================================================
// 
// creepMiner - Burstcoin cryptocurrency CPU and GPU miner
// Copyright (C)  2016-2018 Creepsky (creepsky@gmail.com)
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110 - 1301  USA
// 
// ==========================================================================

#pragma once

#include <memory>
#include <string>
#include "Response.hpp"
#include <Poco/Net/HTTPClientSession.h>

namespace Poco {namespace Net {
	class HTTPRequest;
}}

namespace Burst
{
	const std::string xPlotfile = "X-Plotfile";
	const std::string xDeadline = "X-Deadline";
	const std::string xCapacity = "X-Capacity";
	const std::string xMiner = "X-Miner";
	const std::string xWorker = "X-Worker";

	class Deadline;

	class Request
	{
	public:
		Request(std::unique_ptr<Poco::Net::HTTPClientSession> session);
		Request(const Request& rhs) = delete;
		Request(Request&& rhs) = default;
		~Request();

		Request& operator=(const Request& rhs) = delete;
		Request& operator=(Request&& rhs) = default;

		bool canSend() const;
		Response send(Poco::Net::HTTPRequest& request);

		std::unique_ptr<Poco::Net::HTTPClientSession> transferSession();

	private:
		std::unique_ptr<Poco::Net::HTTPClientSession> session_;
	};

	class NonceRequest
	{
	public:
		NonceRequest(std::unique_ptr<Poco::Net::HTTPClientSession> socket);

		NonceResponse submit(const Deadline& deadline);

		std::unique_ptr<Poco::Net::HTTPClientSession> transferSession();

	private:
		Request request_;
	};
}
