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

#include "gpu_cuda_impl.hpp"
#include "logging/MinerLogger.hpp"
#include "shabal/cuda/Shabal.hpp"
#include "mining/MinerData.hpp"
#include "Declarations.hpp"

#define check(x) if (!x) { log_critical(MinerLogger::plotVerifier, "Error on %s", std::string(#x)); return false; }

bool Burst::Gpu_Cuda_Impl::allocateMemory(void** memory, MemoryType type, size_t size)
{
	check(cuda_alloc_memory(type, size, memory));
	return true;
}

bool Burst::Gpu_Cuda_Impl::verify(const GensigData* gpuGensig, ScoopData* gpuScoops, Poco::UInt64* gpuDeadlines,
	size_t nonces, Poco::UInt64 nonceStart, Poco::UInt64 baseTarget)
{
	std::string errorString;
	check(cuda_calculate_shabal_host_preallocated(gpuScoops, gpuDeadlines, nonces, gpuGensig, nonceStart, baseTarget, errorString));
	return true;
}

bool Burst::Gpu_Cuda_Impl::getMinDeadline(Poco::UInt64* gpuDeadlines, size_t size, Poco::UInt64& minDeadline, Poco::UInt64& minDeadlineIndex)
{
	std::string errorString;
	check(cuda_reduce_best_deadline(gpuDeadlines, size, minDeadline, minDeadlineIndex, errorString));
	return true;
}

bool Burst::Gpu_Cuda_Impl::freeMemory(void* memory)
{
	check(cuda_free_memory(memory));
	return true;
}

bool Burst::Gpu_Cuda_Impl::getError(std::string& errorString)
{
	return cuda_get_error(errorString);
}

bool Burst::Gpu_Cuda_Impl::copyMemory(const void* input, void* output, MemoryType type, size_t size, MemoryCopyDirection direction)
{
	check(cuda_copy_memory(type, size, input, output, direction));
	return true;
}
