#pragma once

#include "Deadline.hpp"
#include <Poco/Timestamp.h>
#include <Poco/Timespan.h>
#include <Poco/JSON/Object.h>
#include <Poco/Mutex.h>
#include <deque>
#include <Poco/NotificationCenter.h>
#include <Poco/Observer.h>

namespace Burst
{
	struct BlockData
	{
		uint64_t block = 0;
		uint64_t scoop = 0;
		uint64_t baseTarget = 0;
		std::string genSig = "";
		std::vector<Poco::JSON::Object> entries;
		std::shared_ptr<const Poco::JSON::Object> lastWinner = nullptr;
		std::shared_ptr<Deadline> bestDeadline = nullptr;
	};

	struct BlockDataChangedNotification : Poco::Notification
	{
		BlockDataChangedNotification(Poco::JSON::Object* blockData)
			: blockData{blockData} {}
		~BlockDataChangedNotification() override = default;
		Poco::JSON::Object* blockData;
	};

	class MinerData
	{
	public:
		void setBestDeadline(std::shared_ptr<Deadline> deadline);
		void startNewBlock(uint64_t block, uint64_t scoop, uint64_t baseTarget, const std::string& genSig);
		void addBlockEntry(Poco::JSON::Object entry);
		void setLastWinner(std::shared_ptr<Poco::JSON::Object> winner) const;
		void addConfirmedDeadline();
		void setTargetDeadline(uint64_t deadline);
		void setBestDeadlineCurrent(std::shared_ptr<Deadline> deadline);
		void addWonBlock();

		std::shared_ptr<const Deadline> getBestDeadlineOverall() const;
		const Poco::Timestamp& getStartTime() const;
		Poco::Timespan getRunTime() const;
		uint64_t getCurrentBlock() const;
		uint64_t getBlocksMined() const;
		uint64_t getBlocksWon() const;
		std::shared_ptr<BlockData> getBlockData();
		std::shared_ptr<const BlockData> getBlockData() const;
		std::shared_ptr<const Poco::JSON::Object> getLastWinner() const;
		std::shared_ptr<const BlockData> getHistoricalBlockData(uint32_t roundsBefore) const;
		std::vector<std::shared_ptr<const BlockData>> getAllHistoricalBlockData() const;
		uint64_t getConfirmedDeadlines() const;
		uint64_t getTargetDeadline() const;
		bool compareToTargetDeadline(uint64_t deadline) const;
		uint64_t getAverageDeadline() const;

		template <typename Observer>
		void addObserverBlockDataChanged(Observer& observer,
			typename Poco::Observer<Observer, BlockDataChangedNotification>::Callback function)
		{
			notifiyBlockDataChanged_.addObserver(Poco::Observer<Observer, BlockDataChangedNotification>{
				observer, function
			});
		}

	private:
		Poco::Timestamp startTime_ = {};
		std::shared_ptr<Deadline> bestDeadlineOverall_ = nullptr;
		uint64_t blocksMined_ = 0;
		uint64_t blocksWon_ = 0;
		uint64_t deadlinesConfirmed_ = 0;
		uint64_t targetDeadline_ = 0;
		std::shared_ptr<BlockData> blockData_ = nullptr;
		std::deque<std::shared_ptr<BlockData>> historicalBlocks_;
		mutable Poco::FastMutex mutex_;
		Poco::NotificationCenter notifiyBlockDataChanged_;
	};
}
