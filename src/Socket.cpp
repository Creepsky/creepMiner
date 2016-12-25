#include "Socket.hpp"
#include "SocketDefinitions.hpp"
#include "MinerLogger.hpp"
#include <sstream>
#include "MinerConfig.hpp"
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

	try
	{
		socket_.connect(Poco::Net::SocketAddress{ip, static_cast<Poco::UInt16>(port)});
	}
	catch(Poco::Exception& exc)
	{
		MinerLogger::write(std::string("error on connecting to host: ") + exc.what());
		return false;
	}

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
		socket_.shutdown();
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

bool Burst::Socket::send(const std::string& data)
{
	if (!isConnected())
		return false;

	auto sendLength = static_cast<int>(data.size());
	auto ptr = data.data();

	// send, until all data is on the other side
	while (sendLength > 0)
	{
		auto result = socket_.sendBytes(ptr, sendLength);

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
		bytesRead = socket_.receiveBytes(&readBuffer[0], readBufferSize - totalBytesRead - 1);
		
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
	auto error = 0;
	socket_.getOption(SOL_SOCKET, SO_ERROR, error);
	return error;
}

std::unique_ptr<Poco::Net::HTTPClientSession> Burst::Socket::createSession() const
{
	return std::make_unique<Poco::Net::HTTPClientSession>(socket_);
}

void Burst::Socket::setTimeoutHelper(float seconds, float* fieldToSet)
{
	auto secondsLong = static_cast<long>(seconds);
	auto microSecondsLong = static_cast<long>(seconds - secondsLong);

	*fieldToSet = seconds;

	if (fieldToSet == &send_timeout_)
	{
		//sendOrReceiveFlag = SO_SNDTIMEO;

		if (isConnected())
			socket_.setSendTimeout({secondsLong, microSecondsLong});
	}
	else if (fieldToSet == &receive_timeout_)
	{
		//sendOrReceiveFlag = SO_RCVTIMEO;
		
		if (isConnected())
			socket_.setReceiveTimeout({secondsLong, microSecondsLong});
	}
}
