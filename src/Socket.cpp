#include "Socket.hpp"
#include "SocketDefinitions.hpp"
#include "MinerLogger.h"
#include <sstream>
#include "MinerConfig.h"
#ifndef WIN32
#include <errno.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

Burst::Socket::Socket(float send_timeout, float receive_timeout)
{
	setSendTimeout(send_timeout);
	setReceiveTimeout(receive_timeout);
}

bool Burst::Socket::connect(const std::string& ip, size_t port)
{
	// disconnect first before new connection
	if (isConnected())
		disconnect();
	
	// initialize connection-data
	struct sockaddr_in sock_data;
	//
	inet_pton(AF_INET, ip.c_str(), &sock_data.sin_addr);
	sock_data.sin_family = AF_INET;
	sock_data.sin_port = htons(static_cast<u_short>(port));
	
	socket_ = socket(AF_INET, SOCK_STREAM, 0);

	if (::connect(socket_, reinterpret_cast<struct sockaddr*>(&sock_data), sizeof(struct sockaddr_in)) != 0)
		return false;

	auto errorNr = getError();

	if (errorNr != 0)
		return false;

	ip_ = ip;
	port_ = port;
	connected_ = true;

	// refresh the timeouts
	setReceiveTimeout(receive_timeout_);
	setSendTimeout(send_timeout_);

	return true;
}

void Burst::Socket::disconnect()
{
	if (isConnected())
	{
		shutdown(socket_, SHUT_RDWR);
		closesocket(socket_);
		connected_ = false;
	}
}

bool Burst::Socket::isConnected() const
{
	return connected_;
}

void Burst::Socket::setSendTimeout(float seconds)
{
	setTimeoutHelper(seconds, &send_timeout_);
}

void Burst::Socket::setReceiveTimeout(float seconds)
{
	setTimeoutHelper(seconds, &receive_timeout_);
}

float Burst::Socket::getSendTimeout() const
{
	return send_timeout_;
}

float Burst::Socket::getReceiveTimeout() const
{
	return receive_timeout_;
}

bool Burst::Socket::send(const std::string& data) const
{
	if (!isConnected())
		return false;

	auto sendLength = static_cast<int>(data.size());
	auto ptr = data.data();

	// send, until all data is on the other side
	while (sendLength > 0)
	{
		auto result = ::send(socket_, ptr, sendLength, MSG_NOSIGNAL);

		if (result < 1)
			break;

		ptr += result;
		sendLength -= result;
	}

	return (sendLength == 0);
}

bool Burst::Socket::receive(std::string& data)
{
	if (!isConnected())
		return false;

#ifdef WIN32
	using ssize_t = long;
#endif

	ssize_t totalBytesRead = 0;
	ssize_t bytesRead;
	constexpr auto readBufferSize = 4096;
	char readBuffer[readBufferSize];
	std::stringstream response;

	do
	{
		bytesRead = recv(socket_, &readBuffer[0], readBufferSize - totalBytesRead - 1, 0);

		if (bytesRead > 0)
		{
			totalBytesRead += bytesRead;
			readBuffer[bytesRead] = 0;
			response << &readBuffer[0];
		}
	}
	while ((bytesRead > 0) &&
		(totalBytesRead < static_cast<int>(readBufferSize) - 1));

	data = response.str();

	// if totalBytesRead is 0, the connection got closed by server
	if (totalBytesRead == 0)
	{
		auto errorNr = getError();

		// dont react to timeout
#ifdef WIN32
		if (errorNr != WSAETIMEDOUT)
#else
        if (errorNr != ETIMEDOUT)
#endif
		MinerLogger::write(std::to_string(errorNr));
		{
			MinerLogger::write("Error while receiving on socket!", TextType::Debug);
			MinerLogger::write("Error-Code: " + std::to_string(errorNr), TextType::Debug);
			MinerLogger::write("Disconnecting socket...", TextType::Debug);
			disconnect();
		}
	}

	return (totalBytesRead > 0);
}

int Burst::Socket::getError() const
{
#ifdef WIN32
	return WSAGetLastError();
#else
	int error = 0;
	socklen_t erroropt = sizeof(error);
	getsockopt(socket_, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &erroropt);
	return error;
#endif
}

void Burst::Socket::setTimeoutHelper(float seconds, float* fieldToSet) const
{
#ifdef WIN32
	auto timeout = static_cast<int>(seconds * 1000);
#else
	struct timeval timeout;
	timeout.tv_sec = static_cast<long>(seconds);
	timeout.tv_usec = static_cast<long>((seconds - timeout.tv_sec) * 1000);
#endif

	*fieldToSet = seconds;
	auto sendOrReceiveFlag = 0;

	if (fieldToSet == &send_timeout_)
		sendOrReceiveFlag = SO_SNDTIMEO;
	else if (fieldToSet == &receive_timeout_)
		sendOrReceiveFlag = SO_RCVTIMEO;

	if (isConnected() && sendOrReceiveFlag != 0)
		if (setsockopt(socket_, SOL_SOCKET, sendOrReceiveFlag, reinterpret_cast<char *>(&timeout), sizeof(timeout)) < 0)
			MinerLogger::write("Failed to set timeout for socket!", TextType::Debug);
}
