#pragma once

#include <Poco/Task.h>
#include <vector>
#include "Declarations.hpp"
#include <Poco/AutoPtr.h>
#include <Poco/Notification.h>
#include <Poco/NotificationQueue.h>

namespace Burst
{
	class Miner;

	struct VerifyNotification : Poco::Notification
	{
		typedef Poco::AutoPtr<VerifyNotification> Ptr;

		std::vector<ScoopData> buffer;
		Poco::UInt64 accountId = 0;
		Poco::UInt64 nonceRead = 0;
		Poco::UInt64 nonceStart = 0;
		std::string inputPath = "";
		Poco::UInt64 block = 0;
		GensigData gensig;
		Poco::UInt64 baseTarget = 0;
	};

	class PlotVerifier : public Poco::Task
	{
	public:
		/**
		 * \brief 1. element: nonce, 2. element: deadline
		 */
		using DeadlineTuple = std::pair<Poco::UInt64, Poco::UInt64>;

		PlotVerifier(Miner &miner, Poco::NotificationQueue& queue);
		void runTask() override;

		static DeadlineTuple verify(std::vector<ScoopData>& buffer, Poco::UInt64 nonceRead, Poco::UInt64 nonceStart, size_t offset,
		                            const GensigData& gensig, Poco::UInt64 baseTarget);

		static void verify(std::vector<ScoopData>& buffer, Poco::UInt64 nonceRead, Poco::UInt64 nonceStart, size_t offset,
		                   const GensigData& gensig, Poco::UInt64 accountId, const std::string& inputPath, Poco::UInt64 baseTarget,
		                   Poco::UInt64 blockheight, Miner& miner);

		static void verify(const DeadlineTuple& deadlineTuple, Poco::UInt64 accountId, const std::string& inputPath, Poco::UInt64 blockheight, Miner& miner);

	private:
		Miner* miner_;
		Poco::NotificationQueue* queue_;
	};
}
