#pragma once

#include <memory>
#include "Declarations.hpp"
#include <Poco/Types.h>
#include "rapidjson/document.h"
#include "Url.hpp"

namespace Poco { namespace Net { class HTTPClientSession; } }

namespace Burst
{
	class Url;

	struct Account
	{
		AccountId id;
		std::string name;
		AccountId rewardRecipient;
	};

	class Wallet
	{
	public:
		Wallet();
		Wallet(const Url& url);
		Wallet(const Wallet& rhs) = delete;
		Wallet(Wallet&& rhs) = default;
		~Wallet();

		bool getWinnerOfBlock(uint64_t block, AccountId& winnerId);
		bool getNameOfAccount(AccountId account, std::string& name);
		bool getRewardRecipientOfAccount(AccountId account, AccountId& rewardRecipient);
		bool getLastBlock(uint64_t& block);
		bool getAccount(AccountId id, Account& account);

		Wallet& operator=(const Wallet& rhs) = delete;
		Wallet& operator=(Wallet&& rhs) = default;

	private:
		bool sendWalletRequest(const std::string& uri, rapidjson::Document& json);

		std::unique_ptr<Poco::Net::HTTPClientSession> walletSession_;
		Url url_;
	};
}
