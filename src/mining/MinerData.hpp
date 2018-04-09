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
#include <Poco/Data/Session.h>

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
		BlockData(Poco::UInt64 blockHeight, Poco::UInt64 baseTarget, const std::string& genSigStr, MinerData* parent = nullptr, Poco::UInt64 blockTargetDeadline = 0);

		std::shared_ptr<Deadline> addDeadline(Poco::UInt64 nonce, Poco::UInt64 deadline,
		                                      const std::shared_ptr<Account>& account, Poco::UInt64 block, const std::string& plotFile);
		void setBaseTarget(Poco::UInt64 baseTarget);
		void setLastWinner(std::shared_ptr<Account> account);
		void setRoundTime(double rTime);
		
		void refreshBlockEntry() const;
		void refreshConfig() const;
		void refreshPlotDirs() const;
		void setProgress(float progressRead, float progressVerification, Poco::UInt64 blockheight);
		void setProgress(const std::string& plotDir, float progress, Poco::UInt64 blockheight);
		void setBlockTime(Poco::UInt64 bTime);

		Poco::UInt64 getBlockheight() const;
		Poco::UInt64 getScoop() const;
		Poco::UInt64 getBasetarget() const;
		Poco::UInt64 getDifficulty() const;
		Poco::UInt64 getBlockTargetDeadline() const;
		std::shared_ptr<Account> getLastWinner() const;
		double getRoundTime() const;
		Poco::UInt64 getBlockTime() const;
		
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
		                                            const std::shared_ptr<Account>& account, Poco::UInt64 block, const std::string
		                                            & plotFile);

		void addMessage(const Poco::Message& message) const;
		void clearEntries() const;

		bool forDeadlines(const std::function<bool(const Deadline&)>& traverseFunction) const;

	protected:
		
		void addBlockEntry(Poco::JSON::Object entry) const;
		void confirmedDeadlineEvent(const std::shared_ptr<Deadline>& deadline);

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
			Poco::UInt64 deadline, const std::shared_ptr<Burst::Account>& account, Poco::UInt64 block, const std::string& plotFile);

		std::atomic<Poco::UInt64> blockHeight_;
		std::atomic<Poco::UInt64> scoop_{};
		std::atomic<Poco::UInt64> baseTarget_;
		std::atomic<Poco::UInt64> blockTargetDeadline_;
		GensigData genSig_{};
		std::string genSigStr_ = "";
		double roundTime_;
		Poco::UInt64 blockTime_{};
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

	template <typename T>
	struct HighscoreValue
	{
		Poco::UInt64 height;
		T value;
	};

	class MinerData : public Poco::ActiveDispatcher
	{
	public:
		MinerData();
		~MinerData() override;
		
		std::shared_ptr<BlockData> startNewBlock(Poco::UInt64 block, Poco::UInt64 baseTarget, const std::string& genSig, Poco::UInt64 blockTargetDeadline);
		void addMessage(const Poco::Message& message);

		std::shared_ptr<Deadline> getBestDeadlineOverall(bool onlyHistorical = false) const;
		const Poco::Timestamp& getStartTime() const;
		Poco::Timespan getRunTime() const;
		Poco::UInt64 getBlocksMined() const;
		Poco::UInt64 getBlocksWon() const;
		std::shared_ptr<BlockData> getBlockData();
		std::shared_ptr<const BlockData> getBlockData() const;
		std::shared_ptr<const BlockData> getHistoricalBlockData(Poco::UInt32 roundsBefore) const;
		std::vector<std::shared_ptr<BlockData>> getAllHistoricalBlockData() const;
		Poco::UInt64 getConfirmedDeadlines() const;
		Poco::UInt64 getAverageDeadline() const;
		Poco::Int64 getDifficultyDifference() const;
		HighscoreValue<Poco::UInt64> getLowestDifficulty() const;
		HighscoreValue<Poco::UInt64> getHighestDifficulty() const;

		Poco::UInt64 getCurrentBlockheight() const;
		Poco::UInt64 getCurrentBasetarget() const;
		Poco::UInt64 getCurrentScoopNum() const;
		Poco::ActiveResult<Poco::UInt64> getWonBlocksAsync(const Wallet& wallet, const Accounts& accounts);

		Poco::BasicEvent<const Poco::JSON::Object> blockDataChangedEvent;
		std::vector<std::shared_ptr<BlockData>> getHistoricalBlocks(Poco::UInt64 from, Poco::UInt64 to) const;

		void forAllBlocks(Poco::UInt64 from, Poco::UInt64 to, const std::function<bool(std::shared_ptr<BlockData>&)>& traverseFunction) const;

	protected:
		Poco::UInt64 runGetWonBlocks(const std::pair<const Wallet*, const Accounts*>& args);

	private:
		Poco::Timestamp startTime_ = {};
		std::atomic<Poco::UInt64> blocksWon_;
		std::shared_ptr<BlockData> blockData_ = nullptr;
		mutable std::mutex mutex_;

		std::unique_ptr<Poco::Data::Session> dbSession_ = nullptr;

		Poco::ActiveMethod<Poco::UInt64, std::pair<const Wallet*, const Accounts*>, MinerData,
						   Poco::ActiveStarter<MinerData>> activityWonBlocks_;

		friend class BlockData;
	};
}
