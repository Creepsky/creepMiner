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

#include "PlotVerifier.hpp"
#include "shabal/MinerShabal.hpp"
#include "mining/Miner.hpp"
#include "logging/MinerLogger.hpp"
#include "PlotReader.hpp"
#include <atomic>
#include "Declarations.hpp"
#include "logging/Performance.hpp"
#include <Poco/Nullable.h>

#if MINING_CUDA || MINING_OPENCL
#include "gpu/gpu_declarations.hpp"
#include "gpu/gpu_shell.hpp"
#include "gpu/algorithm/gpu_algorithm_atomic.hpp"
#endif

Burst::PlotVerifier::PlotVerifier(Miner &miner, Poco::NotificationQueue& queue, std::shared_ptr<PlotReadProgress> progress)
	: Task("PlotVerifier"), miner_{&miner}, queue_{&queue}, progress_{progress}
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

		Poco::Nullable<DeadlineTuple> bestResult;

		START_PROBE("PlotVerifier.SearchDeadline")
#if MINING_CUDA || MINING_OPENCL
		DeadlineTuple bestDeadline;
		bestDeadline.first = 0;
		bestDeadline.second = 0;
		Gpu::run<Gpu_Algorithm_Atomic>(
			verifyNotification->buffer,
			verifyNotification->gensig,
			verifyNotification->nonceStart + verifyNotification->nonceRead,
			verifyNotification->baseTarget,
			bestDeadline);
		bestResult = bestDeadline;
#else
		for (size_t i = 0;
			i < verifyNotification->buffer.size() &&
			!isCancelled() &&
			verifyNotification->block == miner_->getBlockheight();
			i += Shabal256::HashSize)
		{
			auto result = verify(verifyNotification->buffer, verifyNotification->nonceRead, verifyNotification->nonceStart, i,
				verifyNotification->gensig,
				verifyNotification->baseTarget);

			for (auto& pair : result)
				// make sure the nonce->deadline pair is valid...
				if (pair.first > 0 && pair.second > 0)
					// ..and better than the others
					if (bestResult.isNull() || pair.second < bestResult.value().second)
						bestResult = pair;
		}
#endif
		TAKE_PROBE("PlotVerifier.SearchDeadline")
		
		if (!bestResult.isNull())
		{
			START_PROBE("PlotVerifier.Submit")
			miner_->submitNonce(bestResult.value().first,
			                    verifyNotification->accountId,
			                    bestResult.value().second,
			                    verifyNotification->block,
			                    verifyNotification->inputPath);
			TAKE_PROBE("PlotVerifier.Submit")
		}
		
		START_PROBE("PlotVerifier.FreeMemory")
		PlotReader::globalBufferSize.free(verifyNotification->memorySize);
		TAKE_PROBE("PlotVerifier.FreeMemory")

		if (progress_ != nullptr)
			progress_->add(verifyNotification->buffer.size() * Settings::PlotSize, verifyNotification->block);
	}

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
	Poco::UInt64 results[Shabal256::HashSize];
	Shabal256 hash;

	// these are the buffer overflow prove arrays
	// instead of directly working with the raw arrays 
	// we create an additional level of indirection
	const char* gensigPtr[Shabal256::HashSize];
	const ScoopData* scoopPtr[Shabal256::HashSize];
	char* targetPtr[Shabal256::HashSize];
	
	// we init the buffer overflow guardians
	for (auto i = 0u; i < Shabal256::HashSize; ++i)
	{
		auto overflow = i + offset >= buffer.size();

		// if the index would cause a buffer overflow, we init it
		// with a nullptr, otherwise with the value
		gensigPtr[i] = overflow ? nullptr : gensig.data();
		scoopPtr[i] = overflow ? nullptr : buffer.data() + offset + i;
		targetPtr[i] = overflow ? nullptr : targets[i].data();
	}

	// these constants are workarounds for some gcc versions
	constexpr auto hashSize = Settings::HashSize;
	constexpr auto scoopSize = Settings::ScoopSize;

	// hash the gensig according to the cpu instruction level
	hash.update(gensigPtr[0],
#if USE_AVX || USE_AVX2 || USE_SSE4
				gensigPtr[1],
				gensigPtr[2],
				gensigPtr[3],
#endif
#if USE_AVX2
				gensigPtr[4],
				gensigPtr[5],
				gensigPtr[6],
				gensigPtr[7],
#endif
		hashSize);

	// hash the scoop according to the cpu instruction level
	hash.update(scoopPtr[0],
#if USE_AVX || USE_AVX2 || USE_SSE4
				scoopPtr[1],
				scoopPtr[2],
				scoopPtr[3],
#endif
#if USE_AVX2
				scoopPtr[4],
				scoopPtr[5],
				scoopPtr[6],
				scoopPtr[7],
#endif
	            scoopSize);

	// digest the hash
	hash.close(targetPtr[0]
#if USE_AVX || USE_AVX2 || USE_SSE4
			   ,targetPtr[1],
				targetPtr[2],
				targetPtr[3]
#endif
#if USE_AVX2
	           ,targetPtr[4],
				targetPtr[5],
				targetPtr[6],
				targetPtr[7]
#endif
	);

	for (auto i = 0u; i < Shabal256::HashSize; ++i)
		memcpy(&results[i], targets[i].data(), sizeof(Poco::UInt64));
	
	// for every calculated deadline we create a pair of {nonce->deadline}
	std::array<DeadlineTuple, Shabal256::HashSize> pairs;

	for (auto i = 0u; i < Shabal256::HashSize; ++i)
		// only set the pair if it was calculated
		if (i + offset < buffer.size())
			pairs[i] = std::make_pair(nonceStart + nonceRead + offset + i, results[i] / baseTarget);

	return pairs;
}
