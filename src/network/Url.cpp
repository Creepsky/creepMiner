// ==========================================================================
// 
// creepMiner - Burstcoin cryptocurrency CPU and GPU miner
// Copyright (C)  2016-2018 Creepsky (creepsky@gmail.com)
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

#include "Url.hpp"
#include "logging/MinerLogger.hpp"
#include <Poco/Net/HostEntry.h>
#include <Poco/Net/HTTPSessionFactory.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/String.h>

Burst::Url::Url(const std::string& url, const std::string& defaultScheme, unsigned short defaultPort)
	: uri_{url}
{
	try
	{
		// if something is wrong with the uri...
		auto invalid = uri_.getScheme().empty() || uri_.getHost().empty() || uri_.getPort() == 0;
		//
		if (invalid && !defaultScheme.empty() && !url.empty())
		{
			// we try to prepend the default scheme and hope that it can get resolved now
			uri_ = defaultScheme + "://" + url + (defaultPort == 0 ? "" : ":" + std::to_string(defaultPort));

			if (uri_.getPort() == 0)
				uri_.setPort(defaultPort);
		}

//		if (!uri_.empty())
//			ip_ = Poco::Net::DNS::resolveOne(uri_.getHost());
	}
	catch (...)
	{}
}

std::string Burst::Url::getCanonical(bool withScheme) const
{
	return (withScheme ? uri_.getScheme() + "://" : "") + uri_.getHost() + ":" + std::to_string(uri_.getPort());
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

Poco::URI& Burst::Url::getUri()
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
		log_warning(MinerLogger::session, "Could not send request to host: unknown protocol '%s'!\n\tURI: %s\n\t%s",
			uri_.getScheme(), getCanonical(), exc.displayText());
		log_current_stackframe(MinerLogger::session);

		return nullptr;
	}
}
