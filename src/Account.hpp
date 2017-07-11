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
		std::string getName();
		Poco::ActiveResult<std::string> getNameAsync(bool reset = false);
		AccountId getRewardRecipient();
		Poco::ActiveResult<AccountId> getRewardRecipientAsync(bool reset = false);
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
		Poco::ActiveMethod<AccountId, bool, Account> getRewardRecipient_;
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
