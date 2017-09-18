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

#include "gpu_opencl_impl.hpp"
#include <CL/cl.h>
cl_int Burst::Gpu_Opencl_Impl::lastError_ = 0;

bool Burst::Gpu_Opencl_Impl::allocateMemory(void** memory, MemoryType type, size_t size)
{
	return true;
}

bool Burst::Gpu_Opencl_Impl::verify(const GensigData* gpuGensig, ScoopData* gpuScoops, Poco::UInt64* gpuDeadlines,
	size_t nonces, Poco::UInt64 nonceStart, Poco::UInt64 baseTarget)
{
	return true;
}

bool Burst::Gpu_Opencl_Impl::getMinDeadline(Poco::UInt64* gpuDeadlines, size_t size, Poco::UInt64& minDeadline, Poco::UInt64& minDeadlineIndex)
{
	return true;
}

bool Burst::Gpu_Opencl_Impl::freeMemory(void* memory)
{
	clReleaseMemObject(static_cast<cl_mem>(memory));
	return true;
}

bool Burst::Gpu_Opencl_Impl::getError(std::string& errorString)
{
	return false;
}

bool Burst::Gpu_Opencl_Impl::copyMemory(const void* input, void* output, MemoryType type, size_t size, MemoryCopyDirection direction)
{
	return true;
}