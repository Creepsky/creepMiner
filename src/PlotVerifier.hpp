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
		uint64_t accountId = 0;
		uint64_t nonceRead = 0;
		uint64_t nonceStart = 0;
		std::string inputPath = "";
		uint64_t block = 0;
		GensigData gensig;
		uint64_t baseTarget = 0;
	};

	class PlotVerifier : public Poco::Task
	{
	public:
		/**
		 * \brief 1. element: nonce, 2. element: deadline
		 */
		using DeadlineTuple = std::pair<uint64_t, uint64_t>;

		PlotVerifier(Miner &miner, Poco::NotificationQueue& queue);
		void runTask() override;

		static DeadlineTuple verify(std::vector<ScoopData>& buffer, uint64_t nonceRead, uint64_t nonceStart, size_t offset,
			const GensigData& gensig, uint64_t baseTarget);

		static void verify(std::vector<ScoopData>& buffer, uint64_t nonceRead, uint64_t nonceStart, size_t offset,
		                   const GensigData& gensig, uint64_t accountId, const std::string& inputPath, uint64_t baseTarget,
		                   uint64_t blockheight, Miner& miner);

		static void verify(const DeadlineTuple& deadlineTuple, uint64_t accountId, const std::string& inputPath, uint64_t blockheight, Miner& miner);

	private:
		Miner* miner_;
		Poco::NotificationQueue* queue_;
	};
}
