#pragma once

#include <memory>
#include <string>
#include "Response.hpp"

namespace Burst
{
	class Socket;

	class Request
	{
	public:
		Request(std::unique_ptr<Socket> socket);

		bool canSend() const;
		Response sendPost(const std::string& url, const std::string& body, const std::string& header);
		Response sendGet(const std::string& url);

		std::unique_ptr<Socket> close();

	private:
		bool send(const std::string& url, const std::string& method, const std::string& body, const std::string& header) const;

		std::unique_ptr<Socket> socket_;
	};
}
