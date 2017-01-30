#pragma once

#include <string>
#include "SocketDefinitions.hpp"
#include "Poco/Net/StreamSocket.h"
#include <memory>
#include <Poco/Net/HTTPClientSession.h>

namespace Burst
{
	class Socket
	{
	public:
		Socket(float send_timeout, float receive_timeout);

		bool connect(const std::string& ip, size_t port);
		void disconnect();

		bool isConnected() const;

		void setSendTimeout(float seconds);
		void setReceiveTimeout(float seconds);

		float getSendTimeout() const;
		float getReceiveTimeout() const;

		bool send(const std::string& data);
		bool receive(std::string& data);

		int getError() const;
		std::unique_ptr<Poco::Net::HTTPClientSession> createSession() const;

	private:
		void setTimeoutHelper(float seconds, float* fieldToSet);

		float send_timeout_ = 5, receive_timeout_ = 5;
		bool connected_ = false;
		std::string ip_;
		size_t port_;
		Poco::Net::StreamSocket socket_;
	};
}
