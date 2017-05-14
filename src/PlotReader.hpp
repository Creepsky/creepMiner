//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include "Declarations.hpp"
#include <Poco/Task.h>
#include <atomic>
#include <Poco/Notification.h>
#include "MinerConfig.hpp"

namespace Poco
{
	class NotificationQueue;
}

namespace Burst
{
	class Miner;
	class PlotFile;
	class PlotReadProgress;

	class GlobalBufferSize
	{
	public:
		void setMax(uint64_t max);
		bool reserve(uint64_t size);
		void free(uint64_t size);
		
		uint64_t getSize() const;
		uint64_t getMax() const;

	private:
		uint64_t size_ = 0;
		uint64_t max_ = 0;
		mutable Poco::FastMutex mutex_;
	};

	struct PlotReadNotification : Poco::Notification
	{
		typedef Poco::AutoPtr<PlotReadNotification> Ptr;
		std::string dir;
		std::vector<std::shared_ptr<PlotFile>> plotList;
		uint64_t scoopNum = 0;
		GensigData gensig;
		uint64_t blockheight = 0;
		uint64_t baseTarget = 0;
		std::vector<std::pair<std::string, std::vector<std::shared_ptr<PlotFile>>>> relatedPlotLists;
		PlotDir::Type type = PlotDir::Type::Sequential;
	};

	class PlotReader : public Poco::Task
	{
	public:
		PlotReader(Miner& miner, std::shared_ptr<PlotReadProgress> progress,
			Poco::NotificationQueue& verificationQueue, Poco::NotificationQueue& plotReadQueue);
		~PlotReader() override = default;

		void runTask() override;

		static GlobalBufferSize globalBufferSize;

	private:
		Miner& miner_;
		std::shared_ptr<PlotReadProgress> progress_;
		Poco::NotificationQueue* verificationQueue_;
		Poco::NotificationQueue* plotReadQueue_;
	};

	class PlotReadProgress
	{
	public:
		void reset(uint64_t blockheight, uintmax_t max);
		void add(uintmax_t value, uint64_t blockheight);
		bool isReady() const;
		uintmax_t getValue() const;
		float getProgress() const;

	private:
		uintmax_t progress_ = 0, max_ = 0;
		uint64_t blockheight_ = 0;
		mutable Poco::Mutex lock_;
	};
}
