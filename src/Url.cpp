#include "Url.hpp"
#include "MinerLogger.hpp"
#include <Poco/Net/HostEntry.h>
#include <Poco/Net/DNS.h>

Burst::Url::Url(const std::string& url)
	: uri_{url}
{
	try
	{
		ip_ = Poco::Net::DNS::resolveOne(uri_.getHost());
	}
	catch (...)
	{}
}

const std::string& Burst::Url::getCanonical() const
{
	return uri_.getHost();
}

std::string Burst::Url::getIp() const
{
	return ip_.toString();
}

uint16_t Burst::Url::getPort() const
{
	return uri_.getPort();
}

const Poco::URI& Burst::Url::getUri() const
{
	return uri_;
}

bool Burst::Url::empty() const
{
	return uri_.getHost().empty() ||
		uri_.getScheme().empty();
}
