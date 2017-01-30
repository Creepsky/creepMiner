#pragma once

#include "Declarations.hpp"

struct CalculatedDeadline
{
	uint64_t deadline;
	uint64_t nonce;
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
extern "C" bool alloc_memory_cuda(MemoryType memType, uint64_t size,  void** mem);
extern "C" bool realloc_memory_cuda(MemoryType memType, uint64_t size,  void** mem);
extern "C" bool copy_memory_cuda(MemoryType memType, uint64_t size, const void* from, void* to, MemoryCopyDirection copyDirection);
extern "C" bool free_memory_cuda(void* mem);
extern "C" uint64_t calc_memory_size(MemoryType memType, uint64_t size);

extern "C" void calculate_shabal_cuda(Burst::ScoopData* buffer, uint64_t len,
	const Burst::GensigData* gensig, CalculatedDeadline* calculatedDeadlines,
	uint64_t nonceStart, uint64_t nonceRead, uint64_t baseTarget);

extern "C" void calculate_shabal_prealloc_cuda(Burst::ScoopData* buffer, uint64_t bufferSize,
	const Burst::GensigData* gensig, CalculatedDeadline* calculatedDeadlines,
	uint64_t nonceStart, uint64_t nonceRead, uint64_t baseTarget, int gridSize, int blockSize);
