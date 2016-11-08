#pragma once

#include <string>

namespace Burst
{
	class Url
	{
	public:
		Url() = default;
		Url(const std::string& url);
		Url(const std::string& canonical, uint32_t port);

		void setUrl(const std::string& canonical, uint32_t port);

		const std::string& getCanonical() const;
		const std::string& getIp() const;
		uint32_t getPort() const;

	private:
		static std::string resolveHostname(const std::string& url);

	private:
		std::string canonical;
		std::string ip;
		uint32_t port;
	};
}
