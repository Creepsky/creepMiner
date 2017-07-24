// ==========================================================================
// 
// creepMiner - Burstcoin cryptocurrency CPU and GPU miner
// Copyright (C)  2016-2017 Creepsky (creepsky@gmail.com)
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110 - 1301  USA
// 
// ==========================================================================

#pragma once

#include "Declarations.hpp"
#include "Deadline.hpp"
#include <memory>
#include "wallet/Account.hpp"
#include "wallet/Wallet.hpp"
#include <Poco/TaskManager.h>
#include "MinerData.hpp"
#include <Poco/NotificationQueue.h>
#include "plots/PlotVerifier.hpp"
#include "WorkerList.hpp"
#include "network/Response.hpp"

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

		void setMiningIntensity(Poco::UInt32 intensity);
		void setMaxPlotReader(Poco::UInt32 max_reader);
		static void setMaxBufferSize(Poco::UInt64 size);
		void rescanPlotfiles();

	private:
		bool getMiningInfo();
		NonceConfirmation submitNonceAsyncImpl(const std::tuple<Poco::UInt64, Poco::UInt64, Poco::UInt64, Poco::UInt64, std::string>& data);
		SubmitResponse addNewDeadline(Poco::UInt64 nonce, Poco::UInt64 accountId, Poco::UInt64 deadline, Poco::UInt64 blockheight, std::string plotFile,
			 std::shared_ptr<Deadline>& newDeadline);
		void shut_down_worker(Poco::ThreadPool& thread_pool, Poco::TaskManager& task_manager, Poco::NotificationQueue& queue) const;

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
		mutable Poco::Mutex worker_mutex_;
	};
}
