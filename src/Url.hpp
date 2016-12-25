#pragma once

#include <string>
#include <Poco/URI.h>
#include <Poco/Net/IPAddress.h>

namespace Burst
{
	class Url
	{
	public:
		Url() = default;
		Url(const std::string& url);

		const std::string& getCanonical() const;
		std::string getIp() const;
		uint16_t getPort() const;
		const Poco::URI& getUri() const;
		bool empty() const;

	private:
		Poco::URI uri_;
		Poco::Net::IPAddress ip_;
	};
}
