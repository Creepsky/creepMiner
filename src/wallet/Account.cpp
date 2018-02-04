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

#include "Account.hpp"
#include <mutex>
#include <Poco/Mutex.h>
#include "Wallet.hpp"
//#include "Wallet.hpp"
#include <thread>
#include "nxt/nxt_address.h"
#include "logging/Message.hpp"
#include "logging/MinerLogger.hpp"

Burst::Account::Account()
	: Account {0}
{}

Burst::Account::Account(AccountId id)
	: id_{id},
	  wallet_{nullptr}
{}

Burst::Account::Account(const Wallet& wallet, AccountId id, bool fetchAll)
	: id_{id},
	  wallet_{&wallet}
{
	if (fetchAll)
	{
		getOrLoadName(true);
		getOrLoadRewardRecipient(true);
		getOrLoadAccountBlocks(true);
	}
}

void Burst::Account::setWallet(const Wallet& wallet)
{
	Poco::Mutex::ScopedLock lock{ mutex_ };
	wallet_ = &wallet;
}

Burst::AccountId Burst::Account::getId() const
{
	Poco::Mutex::ScopedLock lock{ mutex_ };
	return id_;
}

template <typename T>
struct DefaultValueHolder
{
	static const T value;
};

template <typename T>
const T DefaultValueHolder<T>::value = T();

template <typename T>
const T& getHelper(Poco::Nullable<T>& val, const Burst::Wallet* wallet, bool reset, Poco::Mutex& mutex, std::function<bool(T&)> fetchFunction)
{
	Poco::ScopedLockWithUnlock<Poco::Mutex> lock{ mutex };

	if (wallet != nullptr && !wallet->isActive())
		return val.value(DefaultValueHolder<T>::value);

	// delete cached name if resetflag is set
	if (reset && !val.isNull())
		val.clear();

	// add name if there is no cached one
	if (val.isNull())
	{
		// because the fetching of the name is async
		// we insert an empty name in the map
		val = T{};

		lock.unlock();

		T fetchedVal;

		if (fetchFunction(fetchedVal))
		{
			Poco::Mutex::ScopedLock innerLock{ mutex };
			val = fetchedVal;
		}
		else
		{
			Poco::Mutex::ScopedLock innerLock{ mutex };
			val.clear();
		}
	}

	Poco::Mutex::ScopedLock innerLock{ mutex };
	return val.value(DefaultValueHolder<T>::value);
}

const std::string& Burst::Account::getName() const
{
	Poco::Mutex::ScopedLock lock{ mutex_ };
	return name_.value(DefaultValueHolder<std::string>::value);
}

Burst::AccountId Burst::Account::getRewardRecipient()
{
	Poco::ScopedLock<Poco::Mutex> lock{ mutex_ };
	return rewardRecipient_.value(DefaultValueHolder<AccountId>::value);
}

const std::vector<Burst::Block>& Burst::Account::getBlocks()
{
	Poco::ScopedLock<Poco::Mutex> lock{ mutex_ };
	return blocks_.value(DefaultValueHolder<std::vector<Block>>::value);
}

Poco::ActiveResult<std::string> Burst::Account::getOrLoadName(bool reset)
{
	return DataLoader::getInstance().getName(std::make_tuple(std::ref(*this), reset));
}

Poco::ActiveResult<Burst::AccountId> Burst::Account::getOrLoadRewardRecipient(bool reset)
{
	return DataLoader::getInstance().getRewardRecipient(std::make_tuple(std::ref(*this), reset));
}

Poco::ActiveResult<std::vector<Burst::Block>> Burst::Account::getOrLoadAccountBlocks(bool reset)
{
	return DataLoader::getInstance().getAccountBlocks(std::make_tuple(std::ref(*this), reset));
}

std::string Burst::Account::getAddress() const
{
	return NxtAddress(getId()).to_string();
}

Poco::JSON::Object::Ptr Burst::Account::toJSON() const
{
	Poco::Mutex::ScopedLock lock{ mutex_ };

	Poco::JSON::Object::Ptr json(new Poco::JSON::Object);

	json->set("numeric", getId());
	json->set("address", getAddress());

	auto name = name_.value("");

	if (!name.empty())
		json->set("name", name);

	auto blocks = blocks_.value({});

	Poco::JSON::Array::Ptr jsonBlocks(new Poco::JSON::Array);
	
	for (auto block : blocks)
		jsonBlocks->add(block);

	json->set("blocks", jsonBlocks);

	return json;
}

Burst::Account::DataLoader::DataLoader()
	: getName{this, &DataLoader::runGetName},
	  getRewardRecipient{this, &DataLoader::runGetRewardRecipient},
	  getAccountBlocks{this, &DataLoader::runGetAccountBlocks}
{}

Burst::Account::DataLoader::~DataLoader() {}

Burst::Account::DataLoader& Burst::Account::DataLoader::getInstance()
{
	static DataLoader dataLoader;
	return dataLoader;
}

std::string Burst::Account::DataLoader::runGetName(const AsyncParameter& parameter)
{
	auto& account = std::get<0>(parameter);
	const auto reset = std::get<1>(parameter);

	return getHelper<std::string>(account.name_, account.wallet_, reset, account.mutex_, [&account](std::string& name)
	{
		return account.wallet_->getNameOfAccount(account.id_, name);
	});
}

Burst::AccountId Burst::Account::DataLoader::runGetRewardRecipient(const AsyncParameter& parameter)
{
	auto& account = std::get<0>(parameter);
	const auto reset = std::get<1>(parameter);
	
	return getHelper<AccountId>(account.rewardRecipient_, account.wallet_, reset, account.mutex_, [&account](AccountId& rewardRecipient)
	{
		return account.wallet_->getRewardRecipientOfAccount(account.id_, rewardRecipient);
	});
}

std::vector<Burst::Block> Burst::Account::DataLoader::runGetAccountBlocks(const AsyncParameter& parameter)
{
	auto& account = std::get<0>(parameter);
	const auto reset = std::get<1>(parameter);
	
	return getHelper<std::vector<Block>>(account.blocks_, account.wallet_, reset, account.mutex_, [&account](std::vector<Block>& blocks)
	{
		return account.wallet_->getAccountBlocks(account.id_, blocks);
	});
}

std::shared_ptr<Burst::Account> Burst::Accounts::getAccount(AccountId id, const Wallet& wallet, bool persistent)
{
	Poco::FastMutex::ScopedLock lock{ mutex_ };

	auto iter = accounts_.find(id);

	// if the account is not in the cache, we have to fetch him
	if (iter == accounts_.end())
	{
		auto account = std::make_shared<Account>(wallet, id, persistent);

		// save the account in the cache if wanted
		if (persistent)
		{
			accounts_.emplace(id, account);
			log_debug(MinerLogger::general, "Cached accounts: %z", accounts_.size());
		}

		return account;
	}

	// account is in the cache already
	return accounts_[id];
}

bool Burst::Accounts::isLoaded(AccountId id) const
{
	Poco::FastMutex::ScopedLock lock{ mutex_ };
	return accounts_.find(id) != accounts_.end();
}

std::vector<std::shared_ptr<Burst::Account>> Burst::Accounts::getAccounts() const
{
	Poco::FastMutex::ScopedLock lock{ mutex_ };

	std::vector<std::shared_ptr<Account>> accounts;

	for (auto& account : accounts_)
		accounts.emplace_back(account.second);

	return accounts;
}
