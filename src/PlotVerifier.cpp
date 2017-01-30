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
#if defined MINING_CUDA
	std::vector<CalculatedDeadline> calculatedDeadlines;
	ScoopData* cudaBuffer = nullptr;
	GensigData* cudaGensig = nullptr;
	CalculatedDeadline* cudaDeadlines = nullptr;
	auto blockSize = 0;
	auto gridSize = 0;

	if (!alloc_memory_cuda(MemoryType::Gensig, 0, reinterpret_cast<void**>(&cudaGensig)))
		MinerLogger::write("Could not allocate memmory for gensig on CUDA GPU!", TextType::Error);
#endif

	while (!isCancelled())
	{
		Poco::Notification::Ptr notification(queue_->waitDequeueNotification());
		VerifyNotification::Ptr verifyNotification;

		if (notification)
			verifyNotification = notification.cast<VerifyNotification>();
		else
			break;

		auto targetDeadline = miner_->getTargetDeadline();

#define check(x) if (!x) MinerLogger::write(std::string("error on ") + #x, TextType::Error);

#if defined MINING_CUDA
		if (calculatedDeadlines.size() < verifyNotification->buffer.size())
		{
			check(realloc_memory_cuda(MemoryType::Buffer, verifyNotification->buffer.size(), reinterpret_cast<void**>(&cudaBuffer)));
			check(realloc_memory_cuda(MemoryType::Deadlines, verifyNotification->buffer.size(), reinterpret_cast<void**>(&cudaDeadlines)));
			calculatedDeadlines.resize(verifyNotification->buffer.size(), CalculatedDeadline{0, 0});
			calc_occupancy_cuda(verifyNotification->buffer.size(), gridSize, blockSize);
		}

		check(copy_memory_cuda(MemoryType::Buffer, verifyNotification->buffer.size(), verifyNotification->buffer.data(), cudaBuffer, MemoryCopyDirection::ToDevice));
		check(copy_memory_cuda(MemoryType::Gensig, 0, &miner_->getGensig(), cudaGensig, MemoryCopyDirection::ToDevice));
		
		calculate_shabal_prealloc_cuda(cudaBuffer, verifyNotification->buffer.size(), cudaGensig, cudaDeadlines,
			verifyNotification->nonceStart, verifyNotification->nonceRead, miner_->getBaseTarget(), gridSize, blockSize);

		check(copy_memory_cuda(MemoryType::Deadlines, verifyNotification->buffer.size(), cudaDeadlines, calculatedDeadlines.data(), MemoryCopyDirection::ToHost));

		CalculatedDeadline* bestDeadline = nullptr;

		for (auto i = 0u; i < verifyNotification->buffer.size(); ++i)
		{
			if (bestDeadline == nullptr || bestDeadline->deadline > calculatedDeadlines[i].deadline)
				bestDeadline = &calculatedDeadlines[i];

			if (calculatedDeadlines[i].deadline == 0)
				MinerLogger::write("zero deadline!", TextType::Debug);
		}

		if (bestDeadline != nullptr && bestDeadline->deadline > 0 && verifyNotification->block == miner_->getBlockheight())
		{
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

#if defined MINING_CUDA
	free_memory_cuda(cudaBuffer);
	free_memory_cuda(cudaDeadlines);
	free_memory_cuda(cudaGensig);
#endif

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
