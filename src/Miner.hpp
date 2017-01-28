//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#pragma once

#include <unordered_map>
#include "MinerShabal.hpp"
#include "Declarations.hpp"
#include "Deadline.hpp"
#include <memory>
#include "Account.hpp"
#include "Wallet.hpp"
#include <Poco/TaskManager.h>
#include <vector>
#include <Poco/JSON/Object.h>
#include "MinerData.hpp"
#include <Poco/NotificationQueue.h>

namespace Poco
{
	namespace Net
	{
		class HTTPClientSession;
	}
}

namespace Burst
{
	class MinerConfig;
	class PlotReadProgress;
	class Deadline;

	class Miner
	{
	public:
		Miner();
		~Miner();

		void run();
		void stop();

		size_t getScoopNum() const;
		uint64_t getBaseTarget() const;
		uint64_t getBlockheight() const;
		uint64_t getTargetDeadline() const;
		const GensigData& getGensig() const;
		void updateGensig(const std::string gensigStr, uint64_t blockHeight, uint64_t baseTarget);
		void submitNonce(uint64_t nonce, uint64_t accountId, uint64_t deadline, std::string plotFile);

		std::shared_ptr<Deadline> getBestSent(uint64_t accountId, uint64_t blockHeight);
		std::shared_ptr<Deadline> getBestConfirmed(uint64_t accountId, uint64_t blockHeight);
		std::vector<Poco::JSON::Object> getBlockData() const;
		MinerData& getData();

	private:
		bool getMiningInfo();

		bool running_ = false;
		MinerData data_;
		Shabal256 hash;
		GensigData gensig_;
		std::unordered_map<AccountId, Deadlines> deadlines_;
		Poco::FastMutex deadlinesLock_;
		std::shared_ptr<PlotReadProgress> progress_;
		uint64_t currentBlockHeight_ = 0u;
		uint64_t currentBaseTarget_ = 0u;
		uint64_t targetDeadline_ = 0u;
		std::unique_ptr<Poco::Net::HTTPClientSession> miningInfoSession_;
		Accounts accounts_;
		Wallet wallet_;
		std::unique_ptr<Poco::TaskManager> nonceSubmitterManager_;
		std::unique_ptr<Poco::TaskManager> plotReaderManager_;
		std::unique_ptr<Poco::TaskManager> verifierManager_;
		Poco::ThreadPool minerThreadPool_;
		Poco::ThreadPool plotReaderThreadPool_;
		Poco::NotificationQueue verificationQueue_;
	};
}
