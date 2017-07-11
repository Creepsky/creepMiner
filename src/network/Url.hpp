// ==========================================================================
// 
// creepMiner - Burstcoin cryptocurrency CPU and GPU miner
// Copyright (C)  2016-2017 Creepsky (creepsky@gmail.com)
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110 - 1301  USA
// 
// ==========================================================================

#pragma once

#include <string>
#include <Poco/URI.h>
#include <Poco/Net/IPAddress.h>
#include <memory>
#include <functional>

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
		Url(const std::string& url, const std::string& defaultScheme = "", unsigned short defaultPort = 0);

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
