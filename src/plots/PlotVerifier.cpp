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

template <typename TShabal, typename TUpdateFunction, typename TCloseFunction>
auto verifyHelper(TShabal&& shabal, std::vector<Burst::ScoopData>& buffer, Poco::UInt64 nonceRead,
	Poco::UInt64 nonceStart, size_t offset,
	const Burst::GensigData& gensig, Poco::UInt64 baseTarget,
	TUpdateFunction& updateFunction, TCloseFunction& closeFunction)
{
	constexpr auto HashSize = TShabal::HashSize;

	std::vector<Burst::HashData> targets(HashSize);
	std::vector<Poco::UInt64> results(HashSize);

	// these are the buffer overflow prove arrays 
	// instead of directly working with the raw arrays  
	// we create an additional level of indirection 
	std::vector<const unsigned char*> scoopPtr(HashSize);
	std::vector<unsigned char*> targetPtr(HashSize);

	// we init the buffer overflow guardians
	for (auto i = 0u; i < HashSize; ++i)
	{
		const auto overflow = i + offset >= buffer.size();

		// if the index would cause a buffer overflow, we init it 
		// with a nullptr, otherwise with the value
		scoopPtr[i] = overflow ? nullptr : reinterpret_cast<unsigned char*>(buffer.data() + offset + i);
		targetPtr[i] = overflow ? nullptr : reinterpret_cast<unsigned char*>(targets[i].data());
	}

	// hash the gensig according to the cpu instruction level
	shabal.update(gensig.data(), Burst::Settings::HashSize);

	// hash the scoop according to the cpu instruction level
	updateFunction(shabal, scoopPtr);

	// digest the hash
	closeFunction(shabal, targetPtr);

	for (auto i = 0u; i < HashSize; ++i)
		memcpy(&results[i], targets[i].data(), sizeof(Poco::UInt64));

	// for every calculated deadline we create a pair of {nonce->deadline}
	std::vector<Burst::PlotVerifier::DeadlineTuple> pairs(HashSize);

	for (auto i = 0u; i < HashSize; ++i)
		// only set the pair if it was calculated
		if (i + offset < buffer.size())
			pairs[i] = std::make_pair(nonceStart + nonceRead + offset + i, results[i] / baseTarget);

	return pairs;
}

template <typename TShabal>
void updateFunction8(TShabal& shabal, std::vector<const unsigned char*>& scoopPtr)
{
	shabal.update(scoopPtr[0], scoopPtr[1], scoopPtr[2], scoopPtr[3],
		scoopPtr[4], scoopPtr[5], scoopPtr[6], scoopPtr[7], Burst::Settings::ScoopSize);
}

template <typename TShabal>
void closeFunction8(TShabal& shabal, std::vector<unsigned char*>& targetPtr)
{
	shabal.close(targetPtr[0], targetPtr[1], targetPtr[2], targetPtr[3],
		targetPtr[4], targetPtr[5], targetPtr[6], targetPtr[7]);
}

template <typename TShabal>
void updateFunction4(TShabal& shabal, std::vector<const unsigned char*>& scoopPtr)
{
	shabal.update(scoopPtr[0], scoopPtr[1], scoopPtr[2], scoopPtr[3], Burst::Settings::ScoopSize);
}

template <typename TShabal>
void closeFunction4(TShabal& shabal, std::vector<unsigned char*>& targetPtr)
{
	shabal.close(targetPtr[0], targetPtr[1], targetPtr[2], targetPtr[3]);
}

template <typename TShabal>
void updateFunction1(TShabal& shabal, std::vector<const unsigned char*>& scoopPtr)
{
	shabal.update(scoopPtr[0], Burst::Settings::ScoopSize);
}

template <typename TShabal>
void closeFunction1(TShabal& shabal, std::vector<unsigned char*>& targetPtr)
{
	shabal.close(targetPtr[0]);
}

auto Burst::PlotVerifier::verify(const std::string& cpuInstructionSet,
	std::vector<ScoopData>& buffer, Poco::UInt64 nonceRead, Poco::UInt64 nonceStart, size_t offset,
	const GensigData& gensig, Poco::UInt64 baseTarget)
{
	if (cpuInstructionSet == "SSE4")
		return verifyHelper(Shabal256_SSE4{}, buffer, nonceRead, nonceStart, offset, gensig, baseTarget,
							updateFunction4<Shabal256_SSE4>, closeFunction4<Shabal256_SSE4>);

	if (cpuInstructionSet == "AVX")
		return verifyHelper(Shabal256_AVX{}, buffer, nonceRead, nonceStart, offset, gensig, baseTarget,
							updateFunction4<Shabal256_AVX>, closeFunction4<Shabal256_AVX>);

	if (cpuInstructionSet == "AVX2")
		return verifyHelper(Shabal256_AVX2{}, buffer, nonceRead, nonceStart, offset, gensig, baseTarget,
							updateFunction8<Shabal256_AVX2>, closeFunction8<Shabal256_AVX2>);

	return verifyHelper(Shabal256_SSE2{}, buffer, nonceRead, nonceStart, offset, gensig, baseTarget,
						updateFunction1<Shabal256_SSE2>, closeFunction1<Shabal256_SSE2>);
}

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
	
	const auto HashSize = []()
	{
		if (MinerConfig::getConfig().getCpuInstructionSet() == "SSE2")
			return Shabal256_SSE2::HashSize;
		
		if (MinerConfig::getConfig().getCpuInstructionSet() == "SSE4")
			return Shabal256_SSE4::HashSize;
		
		if (MinerConfig::getConfig().getCpuInstructionSet() == "AVX")
			return Shabal256_AVX::HashSize;

		if (MinerConfig::getConfig().getCpuInstructionSet() == "AVX2")
			return Shabal256_AVX2::HashSize;

		return size_t();
	}();

	auto cpuInstructionSet = MinerConfig::getConfig().getCpuInstructionSet();

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
			i += HashSize)
		{
			auto result = verify(cpuInstructionSet,
				verifyNotification->buffer, verifyNotification->nonceRead, verifyNotification->nonceStart, i,
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
