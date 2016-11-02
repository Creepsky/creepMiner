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
#include "MinerShabal.h"
#include <mutex>
#include "Declarations.hpp"
#include <condition_variable>
#include <cmath>

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

		size_t nonceStart;
		size_t scoopNum;
		size_t nonceCount;
		size_t nonceOffset;
		size_t nonceRead;
		size_t staggerSize;
		uint64_t accountId;
		GensigData gensig;

		bool done;
		bool runVerify;
		std::string inputPath;

		Miner* miner;
		std::thread readerThreadObj;

		std::vector<ScoopData> buffer[2];

		Shabal256 hash;
		bool verifySignaled;
		std::mutex verifyMutex;
		std::condition_variable verifySignal;
		std::vector<ScoopData>* readBuffer;
		std::vector<ScoopData>* writeBuffer;
	};

	class PlotListReader
	{
	public:
		PlotListReader(Miner& miner, std::shared_ptr<PlotReadProgress> progress);
		~PlotListReader();

		void read(std::string&& dir, std::vector<std::shared_ptr<PlotFile>>&& plotFiles);
		void stop();
		bool isDone() const;

	private:
		void readThread();

		std::vector<std::shared_ptr<PlotFile>> plotFileList;
		std::thread readerThreadObj;
		bool done, stopped;
		Miner* miner;
		std::shared_ptr<PlotReadProgress> progress;
		std::string dir;
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
		uintmax_t progress = 0, max = 0;
		std::mutex lock;
	};
}
