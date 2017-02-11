#include "Url.hpp"
#include "MinerLogger.hpp"
#include <Poco/Net/HostEntry.h>
#include <Poco/Net/DNS.h>
#include <Poco/Net/HTTPSessionFactory.h>
#include <Poco/Net/HTTPClientSession.h>

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

std::string Burst::Url::getCanonical(bool withScheme) const
{
	return (withScheme ? uri_.getScheme() + "://" : "") + uri_.getHost();
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

std::unique_ptr<Poco::Net::HTTPClientSession> Burst::Url::createSession() const
{
	try
	{
		return std::unique_ptr<Poco::Net::HTTPClientSession> {
			Poco::Net::HTTPSessionFactory::defaultFactory().createClientSession(uri_)
		};
	}
	catch (...)
	{
		log_warning(MinerLogger::session, "Could not send request to host: unknown protocol '%s'!\nURI: %s",
			uri_.getScheme(), getCanonical());

		return nullptr;
	}
}
