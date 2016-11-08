#include "Url.hpp"
#include "MinerUtil.h"
#include <ws2tcpip.h>
#include "MinerLogger.h"

Burst::Url::Url(const std::string& url)
{
	std::string _canonical;
	auto _port = 0u;
	auto startPos = url.find("//");

	if (startPos == std::string::npos)
		startPos = 0;

	auto hostEnd = url.find(":", startPos);

	if (hostEnd == std::string::npos)
	{
		_port = 80;
		_canonical = url.substr(startPos + 2, url.size() + 2 - startPos);
	}
	else
	{
		_canonical = url.substr(startPos, hostEnd - startPos);
		auto poolPortStr = url.substr(hostEnd + 1, url.size() - (hostEnd + 1));

		try
		{
			_port = std::stoul(poolPortStr);
		}
		catch (...)
		{
			_port = 80;
		}
	}

	setUrl(_canonical, _port);
}

Burst::Url::Url(const std::string& canonical, uint32_t port)
{
	setUrl(canonical, port);
}

void Burst::Url::setUrl(const std::string& canonical, uint32_t port)
{
	this->canonical = canonical;
	this->port = port;
	this->ip = resolveHostname(this->canonical);
}

const std::string& Burst::Url::getCanonical() const
{
	return canonical;
}

const std::string& Burst::Url::getIp() const
{
	return ip;
}

uint32_t Burst::Url::getPort() const
{
	return port;
}

std::string Burst::Url::resolveHostname(const std::string& url)
{
	struct addrinfo* result = nullptr;
	struct sockaddr_in* addr;

	auto retval = getaddrinfo(url.c_str(), nullptr, nullptr, &result);

	if (retval != 0)
		return "";

	char buf[INET_ADDRSTRLEN];
	addr = reinterpret_cast<struct sockaddr_in*>(result->ai_addr);

	if (inet_ntop(AF_INET, &addr->sin_addr, buf, INET_ADDRSTRLEN) == nullptr)
	{
		MinerLogger::write("can not resolve hostname " + url, TextType::Error);
		return "";
	}

	freeaddrinfo(result);

	return buf;
}
