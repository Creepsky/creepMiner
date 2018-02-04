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
#include <vector>
#include <Poco/Net/HTTPClientSession.h>

namespace Burst
{
	class Response
	{
	public:
		Response(std::unique_ptr<Poco::Net::HTTPClientSession> session);
		Response(const Response& rhs) = delete;
		Response(Response&& rhs) = default;
		~Response();

		Response& operator=(const Response& rhs) = delete;
		Response& operator=(Response&& rhs) = default;

		bool canReceive() const;
		bool receive(std::string& data);

		std::unique_ptr<Poco::Net::HTTPClientSession> transferSession();
		const Poco::Exception* getLastError() const;
		bool isDataThere() const;

	private:
		std::unique_ptr<Poco::Net::HTTPClientSession> session_;
	};

	enum class SubmitResponse
	{
		Confirmed,
		Submitted,
		NotBest,
		TooHigh,
		WrongBlock,
		Error,
		Found,
		None
	};

	struct NonceConfirmation
	{
		Poco::UInt64 deadline;
		SubmitResponse errorCode;
		std::string json;
	};

	class NonceResponse
	{
	public:
		NonceResponse(std::unique_ptr<Poco::Net::HTTPClientSession> session);

		bool canReceive() const;
		NonceConfirmation getConfirmation();
		bool isDataThere();

		std::unique_ptr<Poco::Net::HTTPClientSession> transferSession();
		const Poco::Exception* getLastError() const;

	private:
		Response response_;
	};

	class HttpResponse
	{
	public:
		HttpResponse(const std::string& response);

		void setResponse(const std::string& response);

		const std::string& getResponse() const;
		const std::string& getStatus() const;
		const std::string& getContentLength() const;
		const std::string& getContentType() const;
		const std::string& getDate() const;
		const std::string& getMessage() const;

	private:
		const std::string& getPart(size_t index) const;

		std::string response_;
		std::vector<std::string> tokens_;
	};
}
