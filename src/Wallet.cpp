#include "Wallet.hpp"
#include "MinerUtil.hpp"
#include <Poco/Net/HTTPClientSession.h>
#include "MinerConfig.hpp"
#include "Request.hpp"
#include <Poco/Net/HTTPRequest.h>
#include <Poco/JSON/Parser.h>
#include <cassert>
#include <Poco/NestedDiagnosticContext.h>
#include "MinerLogger.hpp"
#include "Account.hpp"

using namespace Poco::Net;

Burst::Wallet::Wallet()
{}

Burst::Wallet::Wallet(const Url& url)
	: url_(url)
{
	poco_ndc(Wallet(const Url& url));
	walletSession_ = url_.createSession();
}

Burst::Wallet::~Wallet()
{}

bool Burst::Wallet::getWinnerOfBlock(uint64_t block, AccountId& winnerId)
{
	poco_ndc(Wallet::getWinnerOfBlock);
	winnerId = 0;

	if (url_.empty())
		return false;

	Poco::JSON::Object::Ptr json;

	if (sendWalletRequest("/burst?requestType=getBlock&height=" + std::to_string(block), json))
	{
		if (json->has("generator"))
		{
			winnerId = json->get("generator").convert<uint64_t>();
			return true;
		}

		return false;
	}

	MinerLogger::write(std::string("Could not get last winner!"), TextType::Debug);
	return false;
}

bool Burst::Wallet::getNameOfAccount(AccountId account, std::string& name)
{
	poco_ndc(Wallet::getNameOfAccount);
	Poco::JSON::Object::Ptr json;
	name = "";

	if (url_.empty())
		return false;

	if (sendWalletRequest("/burst?requestType=getAccount&account=" + std::to_string(account), json))
	{
		if (json->has("name"))
		{
			name = json->get("name").convert<std::string>();
			return true;
		}

		return false;
	}

	MinerLogger::write(std::string("Could not get name of account!"), TextType::Debug);
	return false;
}

bool Burst::Wallet::getRewardRecipientOfAccount(AccountId account, AccountId& rewardRecipient)
{
	poco_ndc(Wallet::getRewardRecipientOfAccount);
	Poco::JSON::Object::Ptr json;
	rewardRecipient = 0;

	if (url_.empty())
		return false;

	if (sendWalletRequest("/burst?requestType=getRewardRecipient&account=" + std::to_string(account), json))
	{
		if (json->has("rewardRecipient"))
		{
			rewardRecipient = json->get("rewardRecipient");
			return true;
		}

		return false;
	}

	MinerLogger::write(std::string("Could not get name of account!"), TextType::Debug);
	return false;
}

bool Burst::Wallet::getLastBlock(uint64_t& block)
{
	poco_ndc(Wallet::getLastBlock);
	Poco::JSON::Object::Ptr json;
	block = 0;

	if (url_.empty())
	{
		return false;
	}

	if (sendWalletRequest("/burst?requestType=getBlock", json))
	{
		if (json->has("height"))
		{
			block = json->get("height");
			return true;
		}

		return false;
	}

	MinerLogger::write(std::string("Could not get last blockheight!"), TextType::Debug);
	return false;
}

void Burst::Wallet::getAccount(AccountId id, Account& account)
{
	if (!url_.empty())
		account = {*this, id};
}

bool Burst::Wallet::sendWalletRequest(const std::string& uri, Poco::JSON::Object::Ptr& json)
{
	poco_ndc(Wallet::sendWalletRequest);
	HTTPRequest request{ HTTPRequest::HTTP_GET, uri, HTTPRequest::HTTP_1_1};
	request.setKeepAlive(true);

	if (walletSession_ == nullptr)
		walletSession_ = url_.createSession();
	
	Request req{std::move(walletSession_)};
	auto resp = req.send(request);
	std::string data;

	if (resp.receive(data))
	{
		try
		{
			Poco::JSON::Parser parser;
			json = parser.parse(data).extract<Poco::JSON::Object::Ptr>();
			transferSession(resp, walletSession_);
			return true;
		}
		catch (Poco::Exception& exc)
		{
			std::vector<std::string> lines = {
				"got invalid json response from server",
				"uri: " + uri,
				"response: " + data
			};
			
			MinerLogger::writeStackframe(lines);
			transferSession(resp, walletSession_);
			return false;
		}
	}

	transferSession(resp, walletSession_);
	transferSession(req, walletSession_);

	assert(walletSession_ != nullptr);

	return false;
}
