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
#include "MinerShabal.hpp"
#include <mutex>
#include "Declarations.hpp"
#include <cmath>
#include <condition_variable>
#include <Poco/Task.h>

namespace Burst
{
	class Miner;
	class PlotFile;
	class PlotReadProgress;

	class PlotReader
	{
	public:
		PlotReader() = default;
		explicit PlotReader(Miner& miner);
		~PlotReader();

		void read(const std::string& path);
		void stop();
		bool isDone() const;

	private:
		void readerThread();
		void verifierThread();

		size_t nonceStart_;
		size_t scoopNum_;
		size_t nonceCount_;
		size_t nonceOffset_;
		size_t nonceRead_;
		size_t staggerSize_;
		uint64_t accountId_;
		GensigData gensig_;

		bool done_ = false, stopped_ = false;
		bool runVerify_;
		std::string inputPath_;

		Miner* miner_;
		std::thread readerThreadObj_;

		std::vector<ScoopData> buffer_[2];

		Shabal256 hash;
		bool verifySignaled_;
		std::mutex verifyMutex_;
		std::condition_variable verifySignal_;
		std::vector<ScoopData>* readBuffer_;
		std::vector<ScoopData>* writeBuffer_;
	};

	class PlotListReader : public Poco::Task
	{
	public:
		PlotListReader(Miner& miner, std::shared_ptr<PlotReadProgress> progress,
			std::string&& dir, std::vector<std::shared_ptr<PlotFile>>&& plotFiles);
		~PlotListReader() override = default;

		void runTask() override;

	private:
		std::vector<std::shared_ptr<PlotFile>> plotFileList_;
		std::thread readerThreadObj_;
		Miner* miner_;
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

	private:
		uintmax_t progress_ = 0, max_ = 0;
		std::mutex lock_;
	};
}
