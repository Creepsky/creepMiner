#pragma once

#include "Deadline.hpp"
#include <Poco/Nullable.h>
#include <unordered_map>

namespace Burst
{
	class Wallet;

	class Account
	{
	public:
		Account();
		Account(Wallet& wallet, AccountId id, bool fetchAll = false);

		void setWallet(Wallet& wallet);

		AccountId getId() const;
		const std::string& getName(bool reset = false);
		const AccountId& getRewardRecipient(bool reset = false);
		Deadlines& getDeadlines();

	private:
		AccountId id_;
		Poco::Nullable<std::string> name_;
		Poco::Nullable<AccountId> rewardRecipient_;
		Deadlines deadlines_;
		Wallet* wallet_;
	};

	class Accounts
	{
	public:
		std::shared_ptr<Account> getAccount(AccountId id, Wallet& wallet, bool persistent);
		bool isLoaded(AccountId id) const;

	private:
		std::unordered_map<AccountId, std::shared_ptr<Account>> accounts_;
	};
}
