#include "Url.hpp"
#include "MinerUtil.h"
#include "MinerLogger.h"
#include <errno.h>
#ifdef WIN32
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <arpa/inet.h>
#endif

Burst::Url::Url(const std::string& url)
{
	std::string canonical;
	unsigned int port;
	auto startPos = url.find("//");

	if (startPos == std::string::npos)
	{
		// maybe there is a single "/" in the url
		// even if the url is not valid with a single "/", we are tolerant ;)
		startPos = url.find('/');
		
		if (startPos == std::string::npos)
			startPos = 0;
		else
			startPos += 1; // 1 is the size of "/"
	}
	// if "//" is in the url, add length of "//" to the start position
	// this would be the case, if for example the url starts with "http://"
	else
		startPos += 2; // 2 is the size of "//"

	// ":" splits the url from the port
	auto hostEnd = url.find(":", startPos);

	// if no port is given, we try it with the default port 8124
	if (hostEnd == std::string::npos)
	{
		port = 8124;
		canonical = url;
	}
	else
	{
		canonical = url.substr(startPos, hostEnd - startPos);
		auto poolPortStr = url.substr(hostEnd + 1, url.size() - (hostEnd + 1));

		try
		{
			port = std::stoul(poolPortStr);
		}
		// if we can not convert the given port to a number, we try it with the default port 8124
		catch (...)
		{
			port = 8124;
		}
	}

	setUrl(canonical, port);
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
	{
		MinerLogger::write("can not resolve hostname " + url, TextType::Error);
		MinerLogger::write("error code: " + std::to_string(retval), TextType::Error);
		return "";
	}

	char buf[INET_ADDRSTRLEN];
	addr = reinterpret_cast<struct sockaddr_in*>(result->ai_addr);

	if (inet_ntop(AF_INET, &addr->sin_addr, buf, INET_ADDRSTRLEN) == nullptr)
	{
		MinerLogger::write("can not resolve hostname " + url, TextType::Error);
		MinerLogger::write("error code: " + std::to_string(errno), TextType::Error);
		return "";
	}

	freeaddrinfo(result);

	return buf;
}
