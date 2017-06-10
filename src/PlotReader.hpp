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
		void setMax(Poco::UInt64 max);
		bool reserve(Poco::UInt64 size);
		void free(Poco::UInt64 size);
		
		Poco::UInt64 getSize() const;
		Poco::UInt64 getMax() const;

	private:
		Poco::UInt64 size_ = 0;
		Poco::UInt64 max_ = 0;
		mutable Poco::FastMutex mutex_;
	};

	struct PlotReadNotification : Poco::Notification
	{
		typedef Poco::AutoPtr<PlotReadNotification> Ptr;
		std::string dir;
		std::vector<std::shared_ptr<PlotFile>> plotList;
		Poco::UInt64 scoopNum = 0;
		GensigData gensig;
		Poco::UInt64 blockheight = 0;
		Poco::UInt64 baseTarget = 0;
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
		void reset(Poco::UInt64 blockheight, uintmax_t max);
		void add(uintmax_t value, Poco::UInt64 blockheight);
		bool isReady() const;
		uintmax_t getValue() const;
		float getProgress() const;

	private:
		uintmax_t progress_ = 0, max_ = 0;
		Poco::UInt64 blockheight_ = 0;
		mutable Poco::Mutex lock_;
	};
}
