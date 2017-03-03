#pragma once

#include "Deadline.hpp"
#include <Poco/Nullable.h>
#include <unordered_map>
#include <Poco/Activity.h>
#include <Poco/ActiveMethod.h>
#include <Poco/JSON/Object.h>

namespace Burst
{
	class Wallet;

	class Account
	{
	public:
		Account();
		Account(AccountId id);
		Account(const Wallet& wallet, AccountId id, bool fetchAll = false);

		void setWallet(const Wallet& wallet);

		AccountId getId() const;
		std::string getName() const;
		Poco::ActiveResult<std::string> getNameAsync(bool reset = false);
		AccountId getRewardRecipient() const;
		//Poco::ActiveResult<AccountId> getRewardRecipientAsync(bool reset = false);
		std::string getAddress() const;

		Poco::JSON::Object::Ptr toJSON() const;

	protected:
		std::string runGetName(const bool& reset);
		AccountId runGetRewardRecipient(const bool& reset);

	private:
		AccountId id_;
		Poco::Nullable<std::string> name_;
		Poco::Nullable<AccountId> rewardRecipient_;
		const Wallet* wallet_;
		Poco::ActiveMethod<std::string, bool, Account> getName_;
		//Poco::ActiveMethod<AccountId, bool, Account> getRewardRecipient_;
		mutable Poco::Mutex mutex_;
	};

	class Accounts
	{
	public:
		std::shared_ptr<Account> getAccount(AccountId id, Wallet& wallet, bool persistent);
		bool isLoaded(AccountId id) const;

	private:
		std::unordered_map<AccountId, std::shared_ptr<Account>> accounts_;
		mutable Poco::FastMutex mutex_;
	};
}
