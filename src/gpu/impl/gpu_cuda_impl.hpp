#pragma once

#include <utility>
#include <Poco/Types.h>
#include "gpu/gpu_declarations.hpp"
#include "mining/MinerData.hpp"

namespace Burst
{
	struct Gpu_Cuda_Impl
	{
		static bool allocateMemory(void** memory, MemoryType type, size_t size);
		static bool copyMemory(const void* input, void* output, MemoryType type, size_t size, MemoryCopyDirection direction);
		static bool verify(const GensigData* gensig, ScoopData* scoops, CalculatedDeadline* deadline, size_t nonces,
			Poco::UInt64 nonceStart, Poco::UInt64 baseTarget, std::pair<Poco::UInt64, Poco::UInt64>& bestDeadline);
		static bool freeMemory(void* memory);
		static bool getError(std::string& errorString);
	};
}
