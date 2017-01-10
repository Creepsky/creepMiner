#pragma once

#include <string>
#include <Poco/URI.h>
#include <Poco/Net/IPAddress.h>
#include <memory>

namespace Poco { namespace Net
{
	class HTTPClientSession;
	class HTTPSessionFactory;
} }

namespace Burst
{
	class Url
	{
	public:
		Url() = default;
		Url(const std::string& url);

		std::string getCanonical(bool withScheme = false) const;
		std::string getIp() const;
		uint16_t getPort() const;
		const Poco::URI& getUri() const;
		bool empty() const;
		std::unique_ptr<Poco::Net::HTTPClientSession> createSession() const;

	private:
		Poco::URI uri_;
		Poco::Net::IPAddress ip_;
	};
}
