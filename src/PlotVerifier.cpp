#include "PlotVerifier.hpp"
#include "MinerShabal.hpp"
#include "Miner.hpp"
#include "MinerLogger.hpp"
#include "PlotReader.hpp"
#include <atomic>
#include "Declarations.hpp"

#ifdef MINING_CUDA 
#include "shabal/cuda/Shabal.hpp"
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
	std::string errorString;

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
		
		if (!calculate_shabal_prealloc_cuda(cudaBuffer, verifyNotification->buffer.size(), cudaGensig, cudaDeadlines,
			verifyNotification->nonceStart, verifyNotification->nonceRead, miner_->getBaseTarget(), gridSize, blockSize, errorString))
			log_error(Burst::MinerLogger::plotVerifier, "CUDA error: %s", errorString);

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
		Poco::Nullable<DeadlineTuple> bestResult;

		for (size_t i = 0; i < verifyNotification->buffer.size() && !isCancelled(); i += Shabal256::HashSize)
		{
			auto result = verify(verifyNotification->buffer, verifyNotification->nonceRead, verifyNotification->nonceStart, i,
				verifyNotification->gensig,
				verifyNotification->baseTarget);

			for (auto& pair : result)
				if (bestResult.isNull() ||
					pair.second < bestResult.value().second)
					bestResult = pair;
		}

		if (!bestResult.isNull())
			miner_->submitNonce(bestResult.value().first,
			                    verifyNotification->accountId,
			                    bestResult.value().second,
			                    verifyNotification->block,
			                    verifyNotification->inputPath);

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

template <size_t size, typename ...T>
void close(Burst::Shabal256& hash, T... args)
{
	hash.close(std::forward<T>(args)...);
}

std::array<Burst::PlotVerifier::DeadlineTuple, Burst::Shabal256::HashSize> Burst::PlotVerifier::verify(
	std::vector<ScoopData>& buffer, Poco::UInt64 nonceRead,
	Poco::UInt64 nonceStart, size_t offset,
	const GensigData& gensig, Poco::UInt64 baseTarget)
{
	HashData targets[Shabal256::HashSize];
	Shabal256 hash;

	constexpr auto hashSize = Settings::HashSize;
	constexpr auto scoopSize = Settings::ScoopSize;

	hash.update(gensig.data(), hashSize);

	hash.update(buffer.data() + offset + 0,
#if __AVX__ || __AVX2__
	            buffer.data() + offset + 1,
	            buffer.data() + offset + 2,
	            buffer.data() + offset + 3,
#endif
#if __AVX2__
	            buffer.data() + offset + 4,
	            buffer.data() + offset + 5,
	            buffer.data() + offset + 6,
	            buffer.data() + offset + 7,
#endif
	            scoopSize);

	hash.close(targets[0].data()
#if __AVX__ || __AVX2__
	           ,targets[1].data(),
	           targets[2].data(),
	           targets[3].data()
#endif
#if __AVX2__
	           ,targets[4].data(),
	           targets[5].data(),
	           targets[6].data(),
	           targets[7].data()
#endif
	);

	Poco::UInt64 results[Shabal256::HashSize];

	for (auto i = 0u; i < Shabal256::HashSize; ++i)
		memcpy(&results[i], targets[i].data(), sizeof(Poco::UInt64));
	
	std::array<DeadlineTuple, Shabal256::HashSize> pairs;

	for (auto i = 0u; i < Shabal256::HashSize; ++i)
		pairs[i] = std::make_pair(nonceStart + nonceRead + offset + i, results[i] / baseTarget);

	return pairs;
}
