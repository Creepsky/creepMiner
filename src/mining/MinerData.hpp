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
#include <Poco/Timestamp.h>
#include <Poco/Timespan.h>
#include <Poco/JSON/Object.h>
#include <Poco/Mutex.h>
#include <deque>
#include <Poco/ActiveDispatcher.h>
#include <Poco/ActiveMethod.h>
#include <unordered_map>
#include <atomic>
#include <functional>
#include <Poco/BasicEvent.h>
#include <Poco/Message.h>

namespace Burst
{
	class MinerData;
	class Accounts;
	class Wallet;
	class Account;

	enum class TargetDeadlineType
	{
		Pool,
		Local,
		Combined
	};

	class BlockData : public Poco::ActiveDispatcher
	{
	public:
		enum class DeadlineSearchType
		{
			Found,
			Sent,
			Confirmed
		};

	public:
		BlockData(Poco::UInt64 blockHeight, Poco::UInt64 baseTarget, std::string genSigStr, MinerData* parent = nullptr);

		std::shared_ptr<Deadline> addDeadline(Poco::UInt64 nonce, Poco::UInt64 deadline,
			std::shared_ptr<Account> account, Poco::UInt64 block, std::string plotFile);
		void setBaseTarget(Poco::UInt64 baseTarget);
		void setLastWinner(std::shared_ptr<Account> account);
		
		void refreshBlockEntry() const;
		void refreshConfig() const;
		void refreshPlotDirs() const;
		void setProgress(float progress, Poco::UInt64 blockheight);
		void setProgress(const std::string& plotDir, float progress, Poco::UInt64 blockheight);

		Poco::UInt64 getBlockheight() const;
		Poco::UInt64 getScoop() const;
		Poco::UInt64 getBasetarget() const;
		std::shared_ptr<Account> getLastWinner() const;
		Poco::ActiveResult<std::shared_ptr<Account>> getLastWinnerAsync(const Wallet& wallet, const Accounts& accounts);
		const GensigData& getGensig() const;
		const std::string& getGensigStr() const;
		std::shared_ptr<Deadline> getBestDeadline() const;
		//std::vector<Poco::JSON::Object> getEntries() const;
		bool forEntries(std::function<bool(const Poco::JSON::Object&)> traverseFunction) const;
		//const std::unordered_map<AccountId, Deadlines>& getDeadlines() const;
		std::shared_ptr<Deadline> getBestDeadline(Poco::UInt64 accountId, DeadlineSearchType searchType);
		
		std::shared_ptr<Deadline> addDeadlineIfBest(Poco::UInt64 nonce, Poco::UInt64 deadline,
			std::shared_ptr<Account> account, Poco::UInt64 block, std::string plotFile);

		void addMessage(const Poco::Message& message) const;
		void clearEntries() const;

	protected:
		std::shared_ptr<Account> runGetLastWinner(const std::pair<const Wallet*, const Accounts*>& args);
		void addBlockEntry(Poco::JSON::Object entry) const;
		void confirmedDeadlineEvent(std::shared_ptr<Deadline> deadline);

	private:
		std::atomic<Poco::UInt64> blockHeight_;
		std::atomic<Poco::UInt64> scoop_;
		std::atomic<Poco::UInt64> baseTarget_;
		GensigData genSig_;
		std::string genSigStr_ = "";
		std::shared_ptr<std::vector<Poco::JSON::Object>> entries_;
		std::shared_ptr<Account> lastWinner_ = nullptr;
		std::unordered_map<AccountId, Deadlines> deadlines_;
		std::shared_ptr<Deadline> bestDeadline_;
		Poco::ActiveMethod<std::shared_ptr<Account>, std::pair<const Wallet*, const Accounts*>, BlockData,
			Poco::ActiveStarter<ActiveDispatcher>> activityLastWinner_;
		MinerData* parent_;
		Poco::JSON::Object::Ptr jsonProgress_;
		std::unordered_map<std::string, Poco::JSON::Object::Ptr> jsonDirProgress_;
		mutable Poco::Mutex mutex_;

		friend class Deadlines;
	};

	class MinerData
	{
	public:
		MinerData();
		
		std::shared_ptr<BlockData> startNewBlock(Poco::UInt64 block, Poco::UInt64 baseTarget, const std::string& genSig);
		void setTargetDeadline(Poco::UInt64 deadline);
		void addMessage(const Poco::Message& message);

		std::shared_ptr<Deadline> getBestDeadlineOverall() const;
		const Poco::Timestamp& getStartTime() const;
		Poco::Timespan getRunTime() const;
		Poco::UInt64 getBlocksMined() const;
		Poco::UInt64 getBlocksWon() const;
		std::shared_ptr<BlockData> getBlockData();
		std::shared_ptr<const BlockData> getBlockData() const;
		std::shared_ptr<const BlockData> getHistoricalBlockData(Poco::UInt32 roundsBefore) const;
		std::vector<std::shared_ptr<const BlockData>> getAllHistoricalBlockData() const;
		Poco::UInt64 getConfirmedDeadlines() const;
		Poco::UInt64 getTargetDeadline(TargetDeadlineType type = TargetDeadlineType::Combined) const;
		bool compareToTargetDeadline(Poco::UInt64 deadline) const;
		Poco::UInt64 getAverageDeadline() const;

		Poco::UInt64 getCurrentBlockheight() const;
		Poco::UInt64 getCurrentBasetarget() const;
		Poco::UInt64 getCurrentScoopNum() const;

		Poco::BasicEvent<const Poco::JSON::Object> blockDataChangedEvent;

	private:
		void addWonBlock();
		void addConfirmedDeadline();
		void setBestDeadline(std::shared_ptr<Deadline> deadline);

		Poco::Timestamp startTime_ = {};
		std::shared_ptr<Deadline> bestDeadlineOverall_ = nullptr;
		std::atomic<Poco::UInt64> blocksMined_;
		std::atomic<Poco::UInt64> blocksWon_;
		std::atomic<Poco::UInt64> deadlinesConfirmed_;
		std::atomic<Poco::UInt64> targetDeadline_;
		std::shared_ptr<BlockData> blockData_ = nullptr;
		std::deque<std::shared_ptr<BlockData>> historicalBlocks_;
		mutable Poco::Mutex mutex_;

		std::atomic<Poco::UInt64> currentBlockheight_;
		std::atomic<Poco::UInt64> currentBasetarget_;
		std::atomic<Poco::UInt64> currentScoopNum_;

		friend class BlockData;
	};
}
