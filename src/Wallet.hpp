#pragma once

#include <memory>
#include <Poco/JSON/Object.h>
#include "Declarations.hpp"
#include "Url.hpp"

namespace Poco
{
	class URI;

	namespace Net
	{
		class HTTPClientSession;
	}
}

namespace Burst
{
	class Account;
	class Url;

	class Wallet
	{
	public:
		Wallet();
		Wallet(const Url& url);
		Wallet(const Wallet& rhs) = delete;
		Wallet(Wallet&& rhs) = default;
		~Wallet();

		bool getWinnerOfBlock(uint64_t block, AccountId& winnerId) const;
		bool getNameOfAccount(AccountId account, std::string& name) const;
		bool getRewardRecipientOfAccount(AccountId account, AccountId& rewardRecipient) const;
		bool getLastBlock(uint64_t& block) const;
		void getAccount(AccountId id, Account& account) const;

		bool isActive() const;

		Wallet& operator=(const Wallet& rhs) = delete;
		Wallet& operator=(Wallet&& rhs) = default;

	private:
		bool sendWalletRequest(const Poco::URI& uri, Poco::JSON::Object::Ptr& json) const;
		Url url_;
	};
}
