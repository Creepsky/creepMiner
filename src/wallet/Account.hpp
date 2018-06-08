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

#pragma once

#include "mining/Deadline.hpp"
#include <Poco/Nullable.h>
#include <unordered_map>
#include <Poco/Activity.h>
#include <Poco/ActiveMethod.h>
#include <Poco/JSON/Object.h>
#include <vector>
#include <Poco/ActiveDispatcher.h>

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
		const std::string& getName() const;
		std::string getAddress() const;
		AccountId getRewardRecipient();
		const std::vector<Block>& getBlocks();

		Poco::ActiveResult<std::string> getOrLoadName(bool reset = false);
		Poco::ActiveResult<AccountId> getOrLoadRewardRecipient(bool reset = false);
		Poco::ActiveResult<std::vector<Block>> getOrLoadAccountBlocks(bool reset = false);

		Poco::JSON::Object::Ptr toJSON() const;
		
	private:
		class DataLoader : public Poco::ActiveDispatcher
		{
		public:
			DataLoader();
			~DataLoader() override;

			static DataLoader& getInstance();

			using AsyncParameter = std::tuple<Account&, bool>;

			Poco::ActiveMethod<std::string, AsyncParameter, DataLoader, Poco::ActiveStarter<ActiveDispatcher>> getName;
			Poco::ActiveMethod<AccountId, AsyncParameter, DataLoader, Poco::ActiveStarter<ActiveDispatcher>> getRewardRecipient;
			Poco::ActiveMethod<std::vector<Block>, AsyncParameter, DataLoader, Poco::ActiveStarter<ActiveDispatcher>> getAccountBlocks;

		private:
			std::string runGetName(const AsyncParameter& parameter);
			AccountId runGetRewardRecipient(const AsyncParameter& parameter);
			std::vector<Block> runGetAccountBlocks(const AsyncParameter& parameter);
		};

	private:
		AccountId id_;
		Poco::Nullable<std::string> name_;
		Poco::Nullable<AccountId> rewardRecipient_;
		Poco::Nullable<std::vector<Block>> blocks_;
		const Wallet* wallet_;
		mutable Poco::Mutex mutex_;
	};

	class Accounts
	{
	public:
		std::shared_ptr<Account> getAccount(AccountId id, const Wallet& wallet, bool persistent);
		bool isLoaded(AccountId id) const;
		std::vector<std::shared_ptr<Account>> getAccounts() const;

	private:
		std::unordered_map<AccountId, std::shared_ptr<Account>> accounts_;
		mutable Poco::Mutex mutex_;
	};
}
