#include "Socket.hpp"
#include "SocketDefinitions.hpp"
#include "MinerLogger.h"
#include <sstream>

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

	if (::connect(socket_, reinterpret_cast<struct sockaddr*>(&sock_data), sizeof(struct sockaddr_in)) != 0)
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

bool Burst::Socket::receive(std::string& data) const
{
	if (!isConnected())
		return false;

	auto totalBytesRead = 0;
	ssize_t bytesRead;
	constexpr auto readBufferSize = 2048;
	char readBuffer[readBufferSize];
	std::stringstream response;

	do
	{
		bytesRead = recv(socket_, &readBuffer[totalBytesRead], readBufferSize - totalBytesRead - 1, 0);

		if (bytesRead > 0)
		{
			totalBytesRead += bytesRead;
			readBuffer[totalBytesRead] = 0;
			response << &readBuffer[0];
			totalBytesRead = 0;
			readBuffer[totalBytesRead] = 0;
		}
	}
	while ((bytesRead > 0) &&
		(totalBytesRead < static_cast<int>(readBufferSize) - 1));

	data = response.str();

	return (!data.empty());
}

void Burst::Socket::setTimeoutHelper(float seconds, float* fieldToSet) const
{
	auto timeout_microseconds = static_cast<long>(seconds * 1000);

	struct timeval timeout;
	timeout.tv_sec = static_cast<long>(seconds);
	timeout.tv_usec = static_cast<long>((seconds - timeout.tv_sec) * 1000);

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
