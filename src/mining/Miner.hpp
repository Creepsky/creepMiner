// ==========================================================================
// 
// creepMiner - Burstcoin cryptocurrency CPU and GPU miner
// Copyright (C)  2016-2018 Creepsky (creepsky@gmail.com)
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
#include "WorkerList.hpp"
#include "network/Response.hpp"
#include <Poco/Timer.h>

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
		void restart();
		static void sigUsr1Handler(int sigNum);
		void addPlotReadNotifications(bool wakeUpCall = false);
		bool wantRestart() const;

		bool hasBlockData() const;
		bool isProcessing() const;
		Poco::UInt64 getScoopNum() const;
		Poco::UInt64 getBaseTarget() const;
		Poco::UInt64 getBlockheight() const;
		const GensigData& getGensig() const;
		const std::string& getGensigStr() const;
		void updateGensig(const std::string& gensigStr, Poco::UInt64 blockHeight, Poco::UInt64 baseTarget);

		std::shared_ptr<Deadline> addDeadline(Deadline deadline, NonceConfirmation& confirmation);
		NonceConfirmation submitDeadline(std::shared_ptr<Deadline> deadline);

		std::shared_ptr<Deadline> getBestSent(Poco::UInt64 accountId, Poco::UInt64 blockHeight) const;
		std::shared_ptr<Deadline> getBestConfirmed(Poco::UInt64 accountId, Poco::UInt64 blockHeight) const;
		MinerData& getData();
		std::shared_ptr<Account> getAccount(AccountId id, bool persistent = false);
		void createPlotVerifiers();

		void setMiningIntensity(unsigned intensity);
		void setMaxPlotReader(unsigned max_reader);
		static void setMaxBufferSize(Poco::UInt64 size);
		void rescanPlotfiles();
		void setIsProcessing(bool isProc);

		bool isPoC2() const;

	private:
		bool getMiningInfo(const Url& url);
		void shutDownWorker(Poco::ThreadPool& threadPool, Poco::TaskManager& taskManager,
		                    Poco::NotificationQueue& queue) const;
		void progressChanged(float& progress);
		void onWakeUp(Poco::Timer& timer);
		void onRoundProcessed(Poco::UInt64 blockHeight, double roundTime);

		bool running_ = false, restart_ = false, isProcessing_ = false;
		MinerData data_;
		std::shared_ptr<PlotReadProgress> progressRead_, progressVerify_;
		std::unique_ptr<Poco::Net::HTTPClientSession> miningInfoSession_;
		Accounts accounts_;
		Wallet wallet_;
		std::unique_ptr<Poco::TaskManager> nonceSubmitterManager_, plotReader_, verifier_;
		Poco::NotificationQueue plotReadQueue_;
		Poco::NotificationQueue verificationQueue_;
		std::unique_ptr<Poco::ThreadPool> verifierPool_, plotReaderPool_;
		Poco::Timer wakeUpTimer_;
		mutable Poco::Mutex workerMutex_;
		std::chrono::high_resolution_clock::time_point startPoint_;
                static Miner *instance;
	};
}
