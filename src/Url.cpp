#include "Url.hpp"
#include "MinerLogger.hpp"
#include <Poco/Net/HostEntry.h>
#include <Poco/Net/DNS.h>
#include <Poco/Net/HTTPSessionFactory.h>
#include <Poco/Net/HTTPClientSession.h>

Burst::Url::Url(const std::string& url, const std::string& defaultScheme, unsigned short defaultPort)
	: uri_{url}
{
	try
	{
		// if something is wrong with the uri...
		auto invalid = uri_.getScheme().empty() || uri_.getHost().empty() || uri_.getPort() == 0;
		//
		if (invalid &&
			!defaultScheme.empty())
			// we try to prepend the default scheme and hope that it can get resolved now
			uri_ = defaultScheme + "://" + url;

		// if nevertheless the port is empty, we set it to the default port
		if (uri_.getPort() == 0)
			uri_.setPort(defaultPort);

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
