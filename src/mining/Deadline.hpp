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
#include "Declarations.hpp"
#include <Poco/Mutex.h>
#include <atomic>
#include <set>

namespace Burst
{
	class Deadlines;
	class Account;
	class BlockData;

	class Deadline : public std::enable_shared_from_this<Deadline>
	{
	public:
		//Deadline(Poco::UInt64 nonce, Poco::UInt64 deadline);
		Deadline(Poco::UInt64 nonce, Poco::UInt64 deadline, std::shared_ptr<Account> account, Poco::UInt64 block, std::string plotFile,
			Deadlines* parent = nullptr);
		Deadline(Deadline&& rhs) = default;
		~Deadline();

		void onTheWay();
		void send();
		void confirm();

		std::string deadlineToReadableString() const;
		Poco::UInt64 getNonce() const;
		Poco::UInt64 getDeadline() const;
		AccountId getAccountId() const;
		std::string getAccountName() const;
		Poco::UInt64 getBlock() const;
		const std::string& getPlotFile() const;
		std::string getMiner() const;
		Poco::UInt64 getTotalPlotsize() const;

		bool isOnTheWay() const;
		bool isSent() const;
		bool isConfirmed() const;

		void setDeadline(Poco::UInt64 deadline);
		void setMiner(const std::string& miner);
		void setTotalPlotsize(Poco::UInt64 plotsize);

		bool operator<(const Deadline& rhs) const;
		bool operator()(const Deadline& lhs, const Deadline& rhs) const;

	private:
		std::shared_ptr<Account> account_;
		std::atomic<Poco::UInt64> block_;
		std::atomic<Poco::UInt64> nonce_;
		std::atomic<Poco::UInt64> deadline_;
		std::string plotFile_ = "";
		std::atomic<bool> onTheWay_;
		std::atomic<bool> sent_;
		std::atomic<bool> confirmed_;
		std::string minerName_ = "";
		Poco::UInt64 plotsize_ = 0;
		Deadlines* parent_;
		mutable Poco::FastMutex mutex_;
	};

	class Deadlines
	{
	public:
		explicit Deadlines(BlockData* parent = nullptr);
		Deadlines(const Deadlines& rhs);

		std::shared_ptr<Deadline> add(Poco::UInt64 nonce, Poco::UInt64 deadline, std::shared_ptr<Account> account, Poco::UInt64 block, std::string plotFile);
		void clear();
		bool confirm(Nonce nonce);
		bool confirm(Nonce nonce, AccountId accountId, Poco::UInt64 block);

		std::shared_ptr<Deadline> getBest() const;
		std::shared_ptr<Deadline> getBestConfirmed() const;
		std::shared_ptr<Deadline> getBestFound() const;
		std::shared_ptr<Deadline> getBestSent() const;

	private:
		void deadlineEvent(std::shared_ptr<Deadline> deadline, const std::string& type) const;
		void deadlineConfirmed(std::shared_ptr<Deadline> deadline) const;
		void resort();

		struct LessThan : std::binary_function<std::shared_ptr<Deadline>, std::shared_ptr<Deadline>, bool>
		{
			bool operator()(const std::shared_ptr<Deadline>& lhs, const std::shared_ptr<Deadline>& rhs) const
			{
				return !(lhs == rhs) && (*lhs < *rhs);
			}
		};

		std::set<std::shared_ptr<Deadline>, LessThan> deadlines_;
		BlockData* parent_;
		mutable Poco::FastMutex mutex_;

		friend class Deadline;
	};
}
