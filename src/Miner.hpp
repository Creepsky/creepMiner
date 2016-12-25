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
#include "MinerShabal.hpp"
#include "Declarations.hpp"
#include <set>
#include "Deadline.hpp"
#include <thread>
#include <memory>
#include "AccountNames.hpp"
#include "Wallet.hpp"

namespace Poco { namespace Net { class HTTPClientSession; } }

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
		uint64_t getTargetDeadline() const;
		const GensigData& getGensig() const;
		void updateGensig(const std::string gensigStr, uint64_t blockHeight, uint64_t baseTarget);
		void submitNonce(uint64_t nonce, uint64_t accountId, uint64_t deadline, std::string plotFile);

		std::shared_ptr<Deadline> getBestSent(uint64_t accountId, uint64_t blockHeight);
		std::shared_ptr<Deadline> getBestConfirmed(uint64_t accountId, uint64_t blockHeight);

	private:
		void nonceSubmitReport(uint64_t nonce, uint64_t accountId, uint64_t deadline);
		bool getMiningInfo();

		bool running_;
		size_t scoopNum_;
		uint64_t baseTarget_;
		uint64_t blockHeight_;
		std::string gensigStr_;
		Shabal256 hash;
		GensigData gensig_;
		std::vector<std::shared_ptr<PlotListReader>> plotReaders_;
		std::unordered_map<AccountId, Deadlines> deadlines_;
		std::mutex deadlinesLock_;
		std::shared_ptr<PlotReadProgress> progress_;
		std::vector<std::thread> sendNonceThreads_;
		uint64_t currentBlockHeight_ = 0u;
		uint64_t currentBaseTarget_ = 0u;
		uint64_t targetDeadline_ = 0u;
		std::unique_ptr<Poco::Net::HTTPClientSession> miningInfoSession_ = nullptr;
		AccountNames accountNames_;
		Wallet wallet_;
	};
}
