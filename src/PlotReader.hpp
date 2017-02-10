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
#include <atomic>

namespace Poco
{
	class NotificationQueue;
}

namespace Burst
{
	class Miner;
	class PlotFile;
	class PlotReadProgress;

	struct PlotReadNotification : Poco::Notification
	{
		typedef Poco::AutoPtr<PlotReadNotification> Ptr;
		std::string dir;
		std::vector<std::shared_ptr<PlotFile>> plotList;
		uint64_t scoopNum = 0;
		GensigData gensig;
		uint64_t blockheight = 0;
	};

	class PlotReader : public Poco::Task
	{
	public:
		PlotReader(Miner& miner, std::shared_ptr<PlotReadProgress> progress,
			Poco::NotificationQueue& verificationQueue, Poco::NotificationQueue& plotReadQueue);
		~PlotReader() override = default;

		void runTask() override;

		static std::atomic_uint_fast64_t sumBufferSize_;

	private:
		Miner& miner_;
		std::shared_ptr<PlotReadProgress> progress_;
		Poco::NotificationQueue* verificationQueue_;
		Poco::NotificationQueue* plotReadQueue_;
	};

	class PlotReadProgress
	{
	public:
		void reset();
		void add(uintmax_t value);
		void set(uintmax_t value);
		void setMax(uintmax_t value);
		bool isReady() const;
		uintmax_t getValue() const;
		float getProgress() const;

	private:
		uintmax_t progress_ = 0, max_ = 0;
		std::mutex lock_;
	};
}
