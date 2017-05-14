#include "PlotVerifier.hpp"
#include "MinerShabal.hpp"
#include "Miner.hpp"
#include "MinerLogger.hpp"
#include "PlotReader.hpp"
#include <atomic>

#ifdef MINING_CUDA 
#include "shabal-cuda/Shabal.hpp"
#endif

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

	//if (!alloc_memory_cuda(MemoryType::Gensig, 0, reinterpret_cast<void**>(&cudaGensig)))
		//log_error(MinerLogger::plotVerifier, "Could not allocate memmory for gensig on CUDA GPU!");
#endif

	while (!isCancelled())
	{
		Poco::Notification::Ptr notification(queue_->waitDequeueNotification());
		VerifyNotification::Ptr verifyNotification;

		if (notification)
			verifyNotification = notification.cast<VerifyNotification>();
		else
			break;
		
#if defined MINING_CUDA
#define check(x) if (!x) log_critical(MinerLogger::plotVerifier, "Error on %s", std::string(#x));

		check(alloc_memory_cuda(MemoryType::Gensig, 0, reinterpret_cast<void**>(&cudaGensig)));
		check(alloc_memory_cuda(MemoryType::Buffer, verifyNotification->buffer.size(), reinterpret_cast<void**>(&cudaBuffer)));
		check(alloc_memory_cuda(MemoryType::Deadlines, verifyNotification->buffer.size(), reinterpret_cast<void**>(&cudaDeadlines)));
		calculatedDeadlines.resize(verifyNotification->buffer.size(), CalculatedDeadline{0, 0});
		calc_occupancy_cuda(static_cast<int>(verifyNotification->buffer.size()), gridSize, blockSize);

		/*if (calculatedDeadlines.size() < verifyNotification->buffer.size())
		{
			check(realloc_memory_cuda(MemoryType::Buffer, verifyNotification->buffer.size(), reinterpret_cast<void**>(&cudaBuffer)));
			check(realloc_memory_cuda(MemoryType::Deadlines, verifyNotification->buffer.size(), reinterpret_cast<void**>(&cudaDeadlines)));
			calculatedDeadlines.resize(verifyNotification->buffer.size(), CalculatedDeadline{0, 0});
			calc_occupancy_cuda(verifyNotification->buffer.size(), gridSize, blockSize);
		}*/

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
				log_trace(MinerLogger::plotVerifier, "zero deadline!");
		}

		if (bestDeadline == nullptr || bestDeadline->deadline == 0)
			log_debug(MinerLogger::plotVerifier, "CUDA processing gave null deadline!\n"
				"\tplotfile = %s\n"
				"\tbuffer.size() = %z\n"
				"\tnonceStart = %Lu\n"
				"\tnonceRead = %Lu",
				verifyNotification->inputPath, verifyNotification->buffer.size(), verifyNotification->nonceStart, verifyNotification->nonceRead
			);

		miner_->submitNonce(bestDeadline->nonce, verifyNotification->accountId, bestDeadline->deadline,
			verifyNotification->block, verifyNotification->inputPath);

		check(free_memory_cuda(cudaBuffer));
		check(free_memory_cuda(cudaGensig));
		check(free_memory_cuda(cudaDeadlines));
		calculatedDeadlines.resize(0);
#else
		for (size_t i = 0; i < verifyNotification->buffer.size() && !isCancelled(); i++)
			verify(verifyNotification->buffer, verifyNotification->nonceRead, verifyNotification->nonceStart, i,
				verifyNotification->gensig, verifyNotification->accountId, verifyNotification->inputPath,
				verifyNotification->baseTarget, verifyNotification->block, *miner_);
#endif
		
		PlotReader::globalBufferSize.free(verifyNotification->buffer.size() * sizeof(ScoopData));
	}

#if defined MINING_CUDA
	free_memory_cuda(cudaBuffer);
	free_memory_cuda(cudaDeadlines);
	free_memory_cuda(cudaGensig);
#endif

	log_debug(MinerLogger::plotVerifier, "Verifier stopped");
}

void Burst::PlotVerifier::verify(std::vector<ScoopData>& buffer, uint64_t nonceRead, uint64_t nonceStart, size_t offset, const GensigData& gensig,
	uint64_t accountId, const std::string& inputPath, uint64_t baseTarget, uint64_t blockheight, Miner& miner)
{
	HashData target;
	Shabal256 hash;

	auto test = buffer.data() + offset;
	hash.update(gensig.data(), Settings::HashSize);
	hash.update(test, Settings::ScoopSize);
	hash.close(&target[0]);

	uint64_t targetResult = 0;
	memcpy(&targetResult, &target[0], sizeof(decltype(targetResult)));
	auto deadline = targetResult / baseTarget;
	
	auto nonceNum = nonceStart + nonceRead + offset;
	miner.submitNonce(nonceNum, accountId, deadline, blockheight, inputPath);
}
