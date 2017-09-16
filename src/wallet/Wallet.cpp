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

#include "Wallet.hpp"
#include "MinerUtil.hpp"
#include <Poco/Net/HTTPClientSession.h>
#include "mining/MinerConfig.hpp"
#include "network/Request.hpp"
#include <Poco/Net/HTTPRequest.h>
#include <Poco/JSON/Parser.h>
#include <cassert>
#include <Poco/NestedDiagnosticContext.h>
#include "logging/MinerLogger.hpp"
#include "Account.hpp"
#include <thread>

using namespace Poco::Net;

Burst::Wallet::Wallet()
{}

Burst::Wallet::Wallet(const Url& url)
	: url_(url)
{
	poco_ndc(Wallet(const Url& url));
}

Burst::Wallet::~Wallet()
{}

bool Burst::Wallet::getWinnerOfBlock(Poco::UInt64 block, AccountId& winnerId) const
{
	poco_ndc(Wallet::getWinnerOfBlock);
	winnerId = 0;

	if (!isActive())
		return false;

	Poco::JSON::Object::Ptr json;

	Poco::URI uri;
	uri.setPath("/burst");
	uri.addQueryParameter("requestType", "getBlock");
	uri.addQueryParameter("height", std::to_string(block));

	if (sendWalletRequest(uri, json))
	{
		if (json->has("generator"))
		{
            winnerId = json->get("generator").convert<Poco::UInt64>();
			return true;
		}

		return false;
	}

	log_debug(MinerLogger::wallet, "Could not get last winner!");
	return false;
}

bool Burst::Wallet::getNameOfAccount(AccountId account, std::string& name) const
{
	poco_ndc(Wallet::getNameOfAccount);
	Poco::JSON::Object::Ptr json;
	name = "";

	if (!isActive())
		return false;

	Poco::URI uri;
	uri.setPath("/burst");
	uri.addQueryParameter("requestType", "getAccount");
	uri.addQueryParameter("account", std::to_string(account));

	if (sendWalletRequest(uri, json))
	{
		if (json->has("name"))
		{
			name = json->get("name").convert<std::string>();
			return true;
		}

		return false;
	}

	log_debug(MinerLogger::wallet, "Could not get name of account!");
	return false;
}

bool Burst::Wallet::getRewardRecipientOfAccount(AccountId account, AccountId& rewardRecipient) const
{
	poco_ndc(Wallet::getRewardRecipientOfAccount);
	Poco::JSON::Object::Ptr json;
	rewardRecipient = 0;

	if (!isActive())
		return false;

	Poco::URI uri;
	uri.setPath("/burst");
	uri.addQueryParameter("requestType", "getRewardRecipient");
	uri.addQueryParameter("account", std::to_string(account));

	if (sendWalletRequest(uri, json))
	{
		if (json->has("rewardRecipient"))
		{
            rewardRecipient = static_cast<Poco::UInt64>(json->get("rewardRecipient"));
			return true;
		}

		return false;
	}

	log_debug(MinerLogger::wallet, "Could not get name of account!");
	return false;
}

bool Burst::Wallet::getLastBlock(Poco::UInt64& block) const
{
	poco_ndc(Wallet::getLastBlock);
	Poco::JSON::Object::Ptr json;
	block = 0;

	if (!isActive())
		return false;

	Poco::URI uri;
	uri.setPath("/burst");
	uri.addQueryParameter("requestType", "getBlock");

	if (sendWalletRequest(uri, json))
	{
		if (json->has("height"))
		{
            block = static_cast<Poco::UInt64>(json->get("height"));
			return true;
		}

		return false;
	}

	log_debug(MinerLogger::wallet, "Could not get last blockheight!");
	return false;
}

void Burst::Wallet::getAccount(AccountId id, Account& account) const
{
	// TODO REWORK
	//if (!url_.empty())
		//account = {*this, id};
}

bool Burst::Wallet::getAccountBlocks(AccountId id, std::vector<Block>& blocks) const
{
	Poco::JSON::Object::Ptr json;
	blocks.clear();

	if (!isActive())
		return false;

	Poco::URI uri;
	uri.setPath("/burst");
	uri.addQueryParameter("requestType", "getAccountBlockIds");
	uri.addQueryParameter("account", std::to_string(id));

	if (sendWalletRequest(uri, json))
	{
		if (json->has("blockIds"))
		{
			auto blockIds = json->getArray("blockIds");

			for (auto block : *blockIds)
			{
				try
				{
					blocks.emplace_back(block.convert<Poco::UInt64>());
				}
				catch (...)
				{
					log_debug(MinerLogger::wallet,
						"Could not convert block-id to number!\n"
						"\tvalue: %s", block.toString());
				}
			}

			return true;
		}

		return false;
	}

	log_debug(MinerLogger::wallet, "Could not get account blocks!");
	return false;
}

bool Burst::Wallet::isActive() const
{
	return !url_.empty();
}

bool Burst::Wallet::sendWalletRequest(const Poco::URI& uri, Poco::JSON::Object::Ptr& json) const
{
	poco_ndc(Wallet::sendWalletRequest);

	if (!isActive())
		return false;

	log_debug(MinerLogger::wallet, "Sending wallet request '%s'", uri.toString());

	HTTPRequest request{ HTTPRequest::HTTP_GET, uri.getPathAndQuery(), HTTPRequest::HTTP_1_1};
	request.setKeepAlive(false);
		
	Request req{ url_.createSession() };
	auto resp = req.send(request);
	std::string data;

	for (auto i = 0u; i < MinerConfig::getConfig().getWalletRequestTries(); ++i)
	{
		if (resp.receive(data))
		{
			log_debug(MinerLogger::wallet, "Got response for wallet request '%s'\n%s", uri.toString(), data);

			try
			{
				Poco::JSON::Parser parser;
				json = parser.parse(data).extract<Poco::JSON::Object::Ptr>();
				return true;
			}
			catch (Poco::Exception&)
			{
				log_error(MinerLogger::wallet, "Got invalid json response from wallet!\n\tURI: %s", uri.getPathAndQuery());
				log_file_only(MinerLogger::wallet, Poco::Message::PRIO_ERROR, TextType::Error, "Got invalid json response from wallet!\n%s", data);
				log_current_stackframe(MinerLogger::wallet);
				return false;
			}
		}

		std::this_thread::sleep_for(std::chrono::seconds(MinerConfig::getConfig().getWalletRequestRetryWaitTime()));
	}

	log_error(MinerLogger::wallet, "Got no response for wallet request '%s'", uri.toString());

	return false;
}
