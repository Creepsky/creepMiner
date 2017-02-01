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
		uint64_t accountId;
		uint64_t nonceRead;
		uint64_t nonceStart;
		std::string inputPath;
		uint64_t block;
		GensigData gensig;
	};

	class PlotVerifier : public Poco::Task
	{
	public:
		PlotVerifier(Miner &miner, Poco::NotificationQueue& queue);
		void runTask() override;

		static void verify(std::vector<ScoopData>& buffer, uint64_t nonceRead, uint64_t nonceStart, size_t offset,
			const GensigData& gensig, uint64_t accountId, const std::string& inputPath, Miner& miner, uint64_t targetDeadline = 0); 

	private:
		Miner* miner_;
		Poco::NotificationQueue* queue_;
	};
}
