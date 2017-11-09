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
#include <mutex>
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

	class BlockData
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
		void setProgress(float progressRead, float progressVerification, Poco::UInt64 blockheight);
		void setProgress(const std::string& plotDir, float progress, Poco::UInt64 blockheight);

		Poco::UInt64 getBlockheight() const;
		Poco::UInt64 getScoop() const;
		Poco::UInt64 getBasetarget() const;
		Poco::UInt64 getDifficulty() const;
		std::shared_ptr<Account> getLastWinner() const;
		
		const GensigData& getGensig() const;
		const std::string& getGensigStr() const;
		std::shared_ptr<Deadline> getBestDeadline() const;
		std::shared_ptr<Deadline> getBestDeadline(DeadlineSearchType searchType) const;
		//std::vector<Poco::JSON::Object> getEntries() const;
		bool forEntries(std::function<bool(const Poco::JSON::Object&)> traverseFunction) const;
		//const std::unordered_map<AccountId, Deadlines>& getDeadlines() const;
		std::shared_ptr<Deadline> getBestDeadline(Poco::UInt64 accountId, DeadlineSearchType searchType);
		Poco::ActiveResult<std::shared_ptr<Account>> getLastWinnerAsync(const Wallet& wallet, Accounts& accounts);

		std::shared_ptr<Deadline> addDeadlineIfBest(Poco::UInt64 nonce, Poco::UInt64 deadline,
			std::shared_ptr<Account> account, Poco::UInt64 block, std::string plotFile);

		void addMessage(const Poco::Message& message) const;
		void clearEntries() const;

	protected:
		
		void addBlockEntry(Poco::JSON::Object entry) const;
		void confirmedDeadlineEvent(std::shared_ptr<Deadline> deadline);

	private:
		class DataLoader : public Poco::ActiveDispatcher
		{
		public:
			DataLoader();
			~DataLoader() override;

			static DataLoader& getInstance();

			Poco::ActiveMethod<std::shared_ptr<Account>, std::tuple<const Wallet&, Accounts&, BlockData&>, DataLoader,
			                   Poco::ActiveStarter<ActiveDispatcher>> getLastWinner;

		private:
			std::shared_ptr<Account> runGetLastWinner(const std::tuple<const Wallet&, Accounts&, BlockData&>& args);
		};

	private:
		std::shared_ptr<Burst::Deadline> getBestDeadlineUnlocked(Poco::UInt64 accountId,
			Burst::BlockData::DeadlineSearchType searchType);
		std::shared_ptr<Burst::Deadline> addDeadlineUnlocked(Poco::UInt64 nonce,
			Poco::UInt64 deadline, std::shared_ptr<Burst::Account> account, Poco::UInt64 block, std::string plotFile);

		std::atomic<Poco::UInt64> blockHeight_;
		std::atomic<Poco::UInt64> scoop_;
		std::atomic<Poco::UInt64> baseTarget_;
		GensigData genSig_;
		std::string genSigStr_ = "";
		std::shared_ptr<std::vector<Poco::JSON::Object>> entries_;
		std::shared_ptr<Account> lastWinner_ = nullptr;
		std::unordered_map<AccountId, std::shared_ptr<Deadlines>> deadlines_;
		std::shared_ptr<Deadline> bestDeadline_;
		MinerData* parent_;
		Poco::JSON::Object::Ptr jsonProgress_;
		std::unordered_map<std::string, Poco::JSON::Object::Ptr> jsonDirProgress_;
		mutable std::mutex mutex_;

		friend class Deadlines;
	};

	class MinerData : public Poco::ActiveDispatcher
	{
	public:
		MinerData();
		~MinerData() override;
		
		std::shared_ptr<BlockData> startNewBlock(Poco::UInt64 block, Poco::UInt64 baseTarget, const std::string& genSig);
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
		Poco::UInt64 getAverageDeadline() const;
		Poco::Int64 getDifficultyDifference() const;

		Poco::UInt64 getCurrentBlockheight() const;
		Poco::UInt64 getCurrentBasetarget() const;
		Poco::UInt64 getCurrentScoopNum() const;
		Poco::ActiveResult<Poco::UInt64> getWonBlocksAsync(const Wallet& wallet, const Accounts& accounts);

		Poco::BasicEvent<const Poco::JSON::Object> blockDataChangedEvent;

	protected:
		Poco::UInt64 runGetWonBlocks(const std::pair<const Wallet*, const Accounts*>& args);

	private:
		void addConfirmedDeadline();
		void setBestDeadline(std::shared_ptr<Deadline> deadline);

		Poco::Timestamp startTime_ = {};
		std::shared_ptr<Deadline> bestDeadlineOverall_ = nullptr;
		std::atomic<Poco::UInt64> blocksMined_;
		std::atomic<Poco::UInt64> blocksWon_;
		std::atomic<Poco::UInt64> deadlinesConfirmed_;
		std::shared_ptr<BlockData> blockData_ = nullptr;
		std::deque<std::shared_ptr<BlockData>> historicalBlocks_;
		mutable std::mutex mutex_;

		std::atomic<Poco::UInt64> currentBlockheight_;
		std::atomic<Poco::UInt64> currentBasetarget_;
		std::atomic<Poco::UInt64> currentScoopNum_;

		Poco::ActiveMethod<Poco::UInt64, std::pair<const Wallet*, const Accounts*>, MinerData,
						   Poco::ActiveStarter<MinerData>> activityWonBlocks_;

		friend class BlockData;
	};
}
