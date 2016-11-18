#pragma once

#include <string>
#include "SocketDefinitions.hpp"

namespace Burst
{
	class Socket
	{
	public:
		Socket() = default;
		Socket(float send_timeout, float receive_timeout);

		bool connect(const std::string& ip, size_t port);
		void disconnect();

		bool isConnected() const;

		void setSendTimeout(float seconds);
		void setReceiveTimeout(float seconds);

		float getSendTimeout() const;
		float getReceiveTimeout() const;

		bool send(const std::string& data) const;
		bool receive(std::string& data) const;

	private:
		void setTimeoutHelper(float seconds, float* fieldToSet) const;

		float send_timeout_ = 5, receive_timeout_ = 5;
		bool connected_ = false;
		std::string ip_;
		size_t port_;
		::SOCKET socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	};
}
