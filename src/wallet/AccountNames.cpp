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

#include "AccountNames.hpp"
#include "Wallet.hpp"
#include <thread>
#include "logger/MinerLogger.hpp"

const std::string& Burst::AccountNames::getName(AccountId accountId, Wallet& wallet, bool reset)
{
	static std::mutex mutexName;
	std::lock_guard<std::mutex> lock(mutexName);

	auto iter = names_.find(accountId);

	// delete cached name if resetflag is set
	if (reset && iter != names_.end())
	{
		names_.erase(iter);
		iter = names_.end();
	}

	// add name if there is no cached one
	if (iter == names_.end())
	{
		// because the fetching of the name is async
		// we insert an empty name in the map
		names_.emplace(accountId, "");

		// start thread to fetch name of account
		std::thread threadName([this, accountId, &wallet]()
		{
			std::string name;

			if (wallet.getNameOfAccount(accountId, name))
			{
				std::lock_guard<std::mutex> nameLock(mutexName);
				names_[accountId] = name;
			}
			else
			{
				auto delIter = names_.find(accountId);
				if (delIter != names_.end())
					names_.erase(delIter);
			}
		});

		threadName.detach();
	}

	// possible values for the name are:
	// - the actual name from the wallet if its set
	// - the cached name from the wallet if its there
	// - the empty name if the name is not set in the wallet
	return names_[accountId];
}

void Burst::AccountNames::reset()
{
	names_.clear();
}
