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
		Request(const Request& rhs) = delete;
		Request(Request&& rhs) = default;
		~Request();

		Request& operator=(const Request& rhs) = delete;
		Request& operator=(Request&& rhs) = default;

		bool canSend() const;
		Response sendPost(const std::string& url, const std::string& body, const std::string& header);
		Response sendGet(const std::string& url);

		std::unique_ptr<Socket> close();

	private:
		bool send(const std::string& url, const std::string& method, const std::string& body, const std::string& header) const;

		std::unique_ptr<Socket> socket_;
	};

	class NonceRequest
	{
	public:
		NonceRequest(std::unique_ptr<Socket> socket);

		NonceResponse submit(uint64_t nonce, uint64_t accountId, uint64_t& deadline);

		std::unique_ptr<Socket> close();

	private:
		Request request_;
	};
}
