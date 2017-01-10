#include "Wallet.hpp"
#include "MinerUtil.hpp"
#include <Poco/Net/HTTPClientSession.h>
#include "MinerConfig.hpp"
#include "Request.hpp"
#include "rapidjson/reader.h"
#include <Poco/Net/HTTPRequest.h>
#include "MinerLogger.hpp"
#include <Poco/NumberParser.h>
#include "rapidjson/stringbuffer.h"

using namespace Poco::Net;

Burst::Wallet::Wallet()
{}

Burst::Wallet::Wallet(const Url& url)
	: url_(url)
{
	walletSession_ = url_.createSession();
}

Burst::Wallet::~Wallet()
{}

bool Burst::Wallet::getWinnerOfBlock(uint64_t block, AccountId& winnerId)
{
	winnerId = 0;

	if (url_.empty())
		return false;

	rapidjson::Document json;

	if (sendWalletRequest("/burst?requestType=getBlock&height=" + std::to_string(block), json))
	{
		if (json.HasMember("generator"))
		{
			winnerId = Poco::NumberParser::parseUnsigned64(json["generator"].GetString());
			return true;
		}

		return false;
	}

	MinerLogger::write(std::string("Could not get last winner!"), TextType::Debug);
	return false;
}

bool Burst::Wallet::getNameOfAccount(AccountId account, std::string& name)
{
	rapidjson::Document json;
	name = "";

	if (url_.empty())
		return false;

	if (sendWalletRequest("/burst?requestType=getAccount&account=" + std::to_string(account), json))
	{
		if (json.HasMember("name"))
		{
			name = json["name"].GetString();
			return true;
		}

		return false;
	}

	MinerLogger::write(std::string("Could not get name of account!"), TextType::Debug);
	return false;
}

bool Burst::Wallet::getRewardRecipientOfAccount(AccountId account, AccountId& rewardRecipient)
{
	rapidjson::Document json;
	rewardRecipient = 0;

	if (url_.empty())
		return false;

	if (sendWalletRequest("/burst?requestType=getRewardRecipient&account=" + std::to_string(account), json))
	{
		if (json.HasMember("rewardRecipient"))
		{
			rewardRecipient = Poco::NumberParser::parseUnsigned64(json["rewardRecipient"].GetString());
			return true;
		}

		return false;
	}

	MinerLogger::write(std::string("Could not get name of account!"), TextType::Debug);
	return false;
}

bool Burst::Wallet::getLastBlock(uint64_t& block)
{
	rapidjson::Document json;
	block = 0;

	if (url_.empty())
		return false;

	if (sendWalletRequest("/burst?requestType=getBlock", json))
	{
		if (json.HasMember("height"))
		{
			block = json["height"].GetUint64();
			return true;
		}

		return false;
	}

	MinerLogger::write(std::string("Could not get last blockheight!"), TextType::Debug);
	return false;
}

bool Burst::Wallet::getAccount(AccountId id, Account& account)
{
	rapidjson::Document json;

	if (url_.empty())
		return false;

	account.id = id;
	auto successName = getNameOfAccount(id, account.name);
	auto successRecipient = getRewardRecipientOfAccount(id, account.rewardRecipient);

	return successName && successRecipient;
}

bool Burst::Wallet::sendWalletRequest(const std::string& uri, rapidjson::Document& json)
{
	HTTPRequest request{ HTTPRequest::HTTP_GET, uri, HTTPRequest::HTTP_1_1};
	request.setKeepAlive(true);

	if (walletSession_ == nullptr)
		walletSession_ = url_.createSession();
	
	Request req{std::move(walletSession_)};
	auto resp = req.send(request);
	std::string data;

	if (resp.receive(data))
	{
		json.Parse<rapidjson::ParseFlag::kParseDefaultFlags>(data.data());

		if (json.GetParseError() == nullptr)
		{
			transferSession(resp, walletSession_);
			return true;
		}
	}

	transferSession(resp, walletSession_);
	transferSession(req, walletSession_);

	assert(walletSession_ != nullptr);

	return false;
}
