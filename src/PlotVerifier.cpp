#include "PlotVerifier.hpp"
#include "MinerShabal.hpp"
#include "Miner.hpp"
#include "MinerLogger.hpp"
#include "MinerConfig.hpp"
#include "shabal-cuda/Shabal.hpp"

Burst::PlotVerifier::PlotVerifier(Miner &miner, Poco::NotificationQueue& queue)
	: Task("PlotVerifier"), miner_{&miner}, queue_{&queue}
{}

void Burst::PlotVerifier::runTask()
{
	while (!isCancelled())
	{
		Poco::Notification::Ptr notification(queue_->waitDequeueNotification());
		VerifyNotification::Ptr verifyNotification;

		if (notification)
			verifyNotification = notification.cast<VerifyNotification>();
		else
			break;

		auto targetDeadline = miner_->getTargetDeadline();

#if defined MINING_CUDA
		std::vector<CalculatedDeadline> calculatedDeadlines{verifyNotification->buffer.size(), CalculatedDeadline{0, 0}};

		calculate_shabal_cuda(verifyNotification->buffer.data(), verifyNotification->buffer.size(), &miner_->getGensig(),
			&calculatedDeadlines[0], verifyNotification->nonceStart, verifyNotification->nonceRead, miner_->getBaseTarget());

		CalculatedDeadline* bestDeadline = nullptr;

		for (auto& deadline : calculatedDeadlines)
			if (bestDeadline == nullptr || bestDeadline->deadline > deadline.deadline)
				bestDeadline = &deadline;

		if (bestDeadline != nullptr && bestDeadline->deadline > 0)
		{
			if (bestDeadline->deadline < targetDeadline)
				miner_->submitNonce(bestDeadline->nonce, verifyNotification->accountId, bestDeadline->deadline, verifyNotification->inputPath);
		}
		else
		{
			auto lines = {
				std::string{"cuda processing gave null deadline!"},
				std::string{"plotfile = " + verifyNotification->inputPath},
				std::string{"buffer.size() = " + std::to_string(verifyNotification->buffer.size())},
				std::string{"nonceStart = " + std::to_string(verifyNotification->nonceStart)},
				std::string{"nonceRead = " + std::to_string(verifyNotification->nonceRead)}
			};

			MinerLogger::write(lines, TextType::Debug);
		}
#else
		for (size_t i = 0; i < verifyNotification->buffer.size() && !isCancelled() && miner_->getBlockheight() == verifyNotification->block; i++)
			verify(verifyNotification->buffer, verifyNotification->nonceRead, verifyNotification->nonceStart, i,
				miner_->getGensig(), verifyNotification->accountId, verifyNotification->inputPath, *miner_, targetDeadline);
#endif
	}

	MinerLogger::write("Verifier stopped", TextType::Debug);
}

void Burst::PlotVerifier::verify(std::vector<ScoopData>& buffer, uint64_t nonceRead, uint64_t nonceStart, size_t offset, const GensigData& gensig,
	uint64_t accountId, const std::string& inputPath, Miner& miner, uint64_t targetDeadline)
{
	HashData target;
	Shabal256 hash;

	auto test = buffer.data() + offset;
	hash.update(gensig.data(), Settings::HashSize);
	hash.update(test, Settings::ScoopSize);
	hash.close(&target[0]);

	uint64_t targetResult = 0;
	memcpy(&targetResult, &target[0], sizeof(decltype(targetResult)));
	auto deadline = targetResult / miner.getBaseTarget();
	
	if (MinerConfig::getConfig().output.nonceFound || targetDeadline > deadline)
	{
		auto nonceNum = nonceStart + nonceRead + offset;
		miner.submitNonce(nonceNum, accountId, deadline, inputPath);
	}
}
