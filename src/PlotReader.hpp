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

namespace Burst
{
	class Miner;
	class PlotFile;
	class PlotReadProgress;

	class PlotReader : public Poco::Task
	{
	public:
		PlotReader(Miner& miner, std::shared_ptr<PlotReadProgress> progress,
			std::string dir, const std::vector<std::shared_ptr<PlotFile>>& plotList);
		~PlotReader() override = default;

		void runTask() override;

	private:
		uint64_t scoopNum_ = 0;
		uint64_t nonceRead_ = 0;
		GensigData gensig_;
		Miner& miner_;
		const std::vector<std::shared_ptr<PlotFile>>* plotList_;
		std::shared_ptr<PlotReadProgress> progress_;
		std::string dir_;
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
