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
	const std::string X_Plotfile = "X-Plotfile";
	const std::string X_Deadline = "X-Deadline";
	const std::string X_PlotsHash = "X-PlotsHash";
	const std::string X_Capacity = "X-Capacity";
	const std::string X_Miner = "X-Miner";

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
