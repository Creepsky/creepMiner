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

#include <utility>
#include <Poco/Types.h>
#include "gpu/gpu_declarations.hpp"
#include "mining/MinerData.hpp"

namespace Burst
{
	struct CalculatedDeadline;

	struct Gpu_Opencl_Impl
	{
		static bool initStream(void** stream);
		static bool allocateMemory(void** memory, MemoryType type, size_t size);
		static bool copyMemory(const void* input, void* output, MemoryType type, size_t size, MemoryCopyDirection direction, void* stream);
		static bool verify(const GensigData* gpuGensig, ScoopData* gpuScoops, Poco::UInt64* gpuDeadlines, size_t nonces,
			Poco::UInt64 nonceStart, Poco::UInt64 baseTarget, void* stream);
		static bool getMinDeadline(Poco::UInt64* gpuDeadlines, size_t size, Poco::UInt64& minDeadline, Poco::UInt64& minDeadlineIndex, void* stream);
		static bool freeMemory(void* memory);
		static bool getError(std::string& errorString);

	private:
		static int lastError_;
	};
}
