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

		Poco::UInt64 getScoopNum() const;
		Poco::UInt64 getBaseTarget() const;
		Poco::UInt64 getBlockheight() const;
		Poco::UInt64 getTargetDeadline() const;
		const GensigData& getGensig() const;
		const std::string& getGensigStr() const;
		void updateGensig(const std::string gensigStr, Poco::UInt64 blockHeight, Poco::UInt64 baseTarget);

		void submitNonce(Poco::UInt64 nonce, Poco::UInt64 accountId, Poco::UInt64 deadline, Poco::UInt64 blockheight, std::string plotFile);
		Poco::ActiveMethod<NonceConfirmation, std::tuple<Poco::UInt64, Poco::UInt64, Poco::UInt64, Poco::UInt64, std::string>, Miner> submitNonceAsync;

		std::shared_ptr<Deadline> getBestSent(Poco::UInt64 accountId, Poco::UInt64 blockHeight);
		std::shared_ptr<Deadline> getBestConfirmed(Poco::UInt64 accountId, Poco::UInt64 blockHeight);
		//std::vector<Poco::JSON::Object> getBlockData() const;
		MinerData& getData();
		std::shared_ptr<Account> getAccount(AccountId id);

	private:
		bool getMiningInfo();
		NonceConfirmation submitNonceAsyncImpl(const std::tuple<Poco::UInt64, Poco::UInt64, Poco::UInt64, Poco::UInt64, std::string>& data);
		SubmitResponse addNewDeadline(Poco::UInt64 nonce, Poco::UInt64 accountId, Poco::UInt64 deadline, Poco::UInt64 blockheight, std::string plotFile,
			 std::shared_ptr<Deadline>& newDeadline);

		bool running_ = false;
		MinerData data_;
		std::shared_ptr<PlotReadProgress> progress_;
		std::unique_ptr<Poco::Net::HTTPClientSession> miningInfoSession_;
		Accounts accounts_;
		Wallet wallet_;
		std::unique_ptr<Poco::TaskManager> nonceSubmitterManager_;
		Poco::NotificationQueue plotReadQueue_;
		Poco::NotificationQueue verificationQueue_;
		std::unique_ptr<WorkerList<PlotVerifier>> verifier_;
		std::unique_ptr<WorkerList<PlotReader>> plotReader_;
	};
}
