//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#pragma once

#include <mutex>
#include <unordered_map>
#include "MinerShabal.h"
#include "Declarations.hpp"
#include "MinerProtocol.h"
#include <set>
#include "Deadline.hpp"
#include <thread>
#include <condition_variable>

namespace Burst
{
	class PlotListReader;
	class MinerConfig;
	class PlotReadProgress;
	class Deadline;

	class Miner
	{
	public:
		Miner() = default;
		~Miner();

		void run();
		void stop();

		size_t getScoopNum() const;
		uint64_t getBaseTarget() const;
		uint64_t getBlockheight() const;
		const GensigData& getGensig() const;
		void updateGensig(const std::string gensigStr, uint64_t blockHeight, uint64_t baseTarget);
		void submitNonce(uint64_t nonce, uint64_t accountId, uint64_t deadline);

		std::shared_ptr<Deadline> getBestSent(uint64_t accountId, uint64_t blockHeight);

	private:
		void nonceSubmitterThread();
		void nonceSubmitReport(uint64_t nonce, uint64_t accountId, uint64_t deadline);

		bool running;
		size_t scoopNum;
		uint64_t baseTarget;
		uint64_t blockHeight;
		std::string gensigStr;
		MinerProtocol protocol;
		Shabal256 hash;
		GensigData gensig;
		std::vector<std::shared_ptr<PlotListReader>> plotReaders;
		std::unordered_map<AccountId, Deadlines> deadlines;
		std::mutex deadlinesLock;
		std::shared_ptr<PlotReadProgress> progress;
		std::vector<std::thread> sendNonceThreads;
		std::condition_variable newBlockIncoming;
		bool submitThreadNotified = false;
	};
}
