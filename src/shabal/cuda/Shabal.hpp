#pragma once

#include "Declarations.hpp"
#include <Poco/Types.h>

struct CalculatedDeadline
{
	Poco::UInt64 deadline;
	Poco::UInt64 nonce;
};

enum class MemoryType
{
	Buffer,
	Gensig,
	Deadlines
};

enum class MemoryCopyDirection
{
	ToHost,
	ToDevice
};

extern "C" void calc_occupancy_cuda(int bufferSize, int& gridSize, int& blockSize);
extern "C" bool alloc_memory_cuda(MemoryType memType, Poco::UInt64 size,  void** mem);
extern "C" bool realloc_memory_cuda(MemoryType memType, Poco::UInt64 size,  void** mem);
extern "C" bool copy_memory_cuda(MemoryType memType, Poco::UInt64 size, const void* from, void* to, MemoryCopyDirection copyDirection);
extern "C" bool free_memory_cuda(void* mem);
extern "C" Poco::UInt64 calc_memory_size(MemoryType memType, Poco::UInt64 size);

extern "C" void calculate_shabal_cuda(Burst::ScoopData* buffer, Poco::UInt64 len,
	const Burst::GensigData* gensig, CalculatedDeadline* calculatedDeadlines,
	Poco::UInt64 nonceStart, Poco::UInt64 nonceRead, Poco::UInt64 baseTarget);

extern "C" bool calculate_shabal_prealloc_cuda(Burst::ScoopData* buffer, Poco::UInt64 bufferSize,
	const Burst::GensigData* gensig, CalculatedDeadline* calculatedDeadlines,
	Poco::UInt64 nonceStart, Poco::UInt64 nonceRead, Poco::UInt64 baseTarget, int gridSize, int blockSize, std::string& errorString);
