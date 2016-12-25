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

std::unique_ptr<Poco::Net::HTTPClientSession> Burst::Url::createSession() const
{
	try
	{
		return std::unique_ptr<Poco::Net::HTTPClientSession> {
			Poco::Net::HTTPSessionFactory::defaultFactory().createClientSession(uri_)
		};
	}
	catch (Poco::Exception& exc)
	{
		auto lines = {
			std::string("could not send request to host: unknown protocol ") + uri_.getScheme() + "!",
			std::string("uri: " + getCanonical())
		};

		MinerLogger::write(lines, TextType::Error);

		return nullptr;
	}
}
