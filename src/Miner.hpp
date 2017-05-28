//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#pragma once

#include "Declarations.hpp"
#include "Deadline.hpp"
#include <memory>
#include "Account.hpp"
#include "Wallet.hpp"
#include <Poco/TaskManager.h>
#include "MinerData.hpp"
#include <Poco/NotificationQueue.h>
#include "PlotVerifier.hpp"
#include "WorkerList.hpp"
#include "Response.hpp"

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

		uint64_t getScoopNum() const;
		uint64_t getBaseTarget() const;
		uint64_t getBlockheight() const;
		uint64_t getTargetDeadline() const;
		const GensigData& getGensig() const;
		const std::string& getGensigStr() const;
		void updateGensig(const std::string gensigStr, uint64_t blockHeight, uint64_t baseTarget);

		void submitNonce(uint64_t nonce, uint64_t accountId, uint64_t deadline, uint64_t blockheight, std::string plotFile);
		Poco::ActiveMethod<NonceConfirmation, std::tuple<uint64_t, uint64_t, uint64_t, uint64_t, std::string>, Miner> submitNonceAsync;

		std::shared_ptr<Deadline> getBestSent(uint64_t accountId, uint64_t blockHeight);
		std::shared_ptr<Deadline> getBestConfirmed(uint64_t accountId, uint64_t blockHeight);
		//std::vector<Poco::JSON::Object> getBlockData() const;
		MinerData& getData();
		std::shared_ptr<Account> getAccount(AccountId id);

	private:
		bool getMiningInfo();
		NonceConfirmation submitNonceAsyncImpl(const std::tuple<uint64_t, uint64_t, uint64_t, uint64_t, std::string>& data);
		SubmitResponse addNewDeadline(uint64_t nonce, uint64_t accountId, uint64_t deadline, uint64_t blockheight, std::string plotFile,
			 std::shared_ptr<Deadline>& newDeadline);

		bool running_ = false;
		MinerData data_;
		std::shared_ptr<PlotReadProgress> progress_;
		std::unique_ptr<Poco::Net::HTTPClientSession> miningInfoSession_;
		Accounts accounts_;
		Wallet wallet_;
		std::unique_ptr<Poco::TaskManager> nonceSubmitterManager_, plot_reader_, verifier_;
		Poco::NotificationQueue plotReadQueue_;
		Poco::NotificationQueue verificationQueue_;
		std::unique_ptr<Poco::ThreadPool> verifier_pool_, plot_reader_pool_;
	};
}
