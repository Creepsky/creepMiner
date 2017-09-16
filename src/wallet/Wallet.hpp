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

#include <memory>
#include <Poco/JSON/Object.h>
#include "Declarations.hpp"
#include "network/Url.hpp"

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

	using Block = Poco::UInt64;

	class Wallet
	{
	public:
		Wallet();
		Wallet(const Url& url);
		Wallet(const Wallet& rhs) = delete;
		Wallet(Wallet&& rhs) = default;
		~Wallet();

		bool getWinnerOfBlock(Poco::UInt64 block, AccountId& winnerId) const;
		bool getNameOfAccount(AccountId account, std::string& name) const;
		bool getRewardRecipientOfAccount(AccountId account, AccountId& rewardRecipient) const;
		bool getLastBlock(Poco::UInt64& block) const;
		void getAccount(AccountId id, Account& account) const;
		bool getAccountBlocks(AccountId id, std::vector<Block>& blocks) const;

		bool isActive() const;

		Wallet& operator=(const Wallet& rhs) = delete;
		Wallet& operator=(Wallet&& rhs) = default;

	private:
		bool sendWalletRequest(const Poco::URI& uri, Poco::JSON::Object::Ptr& json) const;
		Url url_;
	};
}
