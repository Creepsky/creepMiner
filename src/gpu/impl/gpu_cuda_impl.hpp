#pragma once

#include <utility>
#include <Poco/Types.h>
#include "gpu/gpu_declarations.hpp"
#include "mining/MinerData.hpp"

namespace Burst
{
	struct GpuCudaImpl
	{
		static bool initStream(void** stream);
		static bool allocateMemory(void** memory, MemoryType type, size_t size);
		static bool copyMemory(const void* input, void* output, MemoryType type, size_t size, MemoryCopyDirection direction, void* stream);
		static bool verify(const GensigData* gpuGensig, ScoopData* gpuScoops, Poco::UInt64* gpuDeadlines, size_t nonces,
			Poco::UInt64 nonceStart, Poco::UInt64 baseTarget, void* stream);
		static bool getMinDeadline(Poco::UInt64* gpuDeadlines, size_t size, Poco::UInt64& minDeadline, Poco::UInt64& minDeadlineIndex, void* stream);
		static bool freeMemory(void* memory);
		static bool getError(std::string& errorString);
		static bool listDevices();
		static bool useDevice(unsigned device);
	};
}
