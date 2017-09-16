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

#include "mining/Deadline.hpp"
#include <Poco/Nullable.h>
#include <unordered_map>
#include <Poco/Activity.h>
#include <Poco/ActiveMethod.h>
#include <Poco/JSON/Object.h>
#include <vector>

namespace Burst
{
	class Wallet;

	using Block = Poco::UInt64;

	class Account
	{
	public:
		Account();
		Account(AccountId id);
		Account(const Wallet& wallet, AccountId id, bool fetchAll = false);

		void setWallet(const Wallet& wallet);

		AccountId getId() const;
		std::string getName();
		std::string getAddress() const;
		AccountId getRewardRecipient();
		std::vector<Block> getBlocks();
		
		Poco::ActiveResult<std::string> getNameAsync(bool reset = false);
		Poco::ActiveResult<AccountId> getRewardRecipientAsync(bool reset = false);
		Poco::ActiveResult<std::vector<Block>> getAccountBlocksAsync(bool reset = false);

		Poco::JSON::Object::Ptr toJSON() const;

	protected:
		std::string runGetName(const bool& reset);
		AccountId runGetRewardRecipient(const bool& reset);
		std::vector<Block> runGetAccountBlocks(const bool& reset);

	private:
		AccountId id_;
		Poco::Nullable<std::string> name_;
		Poco::Nullable<AccountId> rewardRecipient_;
		Poco::Nullable<std::vector<Block>> blocks_;
		const Wallet* wallet_;
		Poco::ActiveMethod<std::string, bool, Account> getName_;
		Poco::ActiveMethod<AccountId, bool, Account> getRewardRecipient_;
		Poco::ActiveMethod<std::vector<Block>, bool, Account> getAccountBlocks_;
		mutable Poco::Mutex mutex_;
	};

	class Accounts
	{
	public:
		std::shared_ptr<Account> getAccount(AccountId id, Wallet& wallet, bool persistent);
		bool isLoaded(AccountId id) const;
		std::vector<std::shared_ptr<Account>> getAccounts() const;

	private:
		std::unordered_map<AccountId, std::shared_ptr<Account>> accounts_;
		mutable Poco::FastMutex mutex_;
	};
}
