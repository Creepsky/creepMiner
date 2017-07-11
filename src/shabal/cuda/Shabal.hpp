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
