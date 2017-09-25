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
#include "gpu/gpu_declarations.hpp"
#include <string>
#include <vector>

extern "C" void cuda_calc_occupancy(int bufferSize, int& gridSize, int& blockSize);
extern "C" bool cuda_alloc_memory(Poco::UInt64 size,  void** mem);
extern "C" bool cuda_copy_memory(Poco::UInt64 size, const void* from, void* to, Burst::MemoryCopyDirection copyDirection);
extern "C" bool cuda_free_memory(void* mem);

extern "C" bool cuda_calculate_shabal_host_preallocated(Burst::ScoopData* buffer, Poco::UInt64* deadlines, Poco::UInt64 bufferSize,
	const Burst::GensigData* gensig,
	Poco::UInt64 nonceStart, Poco::UInt64 baseTarget, std::string& errorString);

extern "C" bool cuda_reduce_best_deadline(Poco::UInt64* deadlines, size_t size,
	Poco::UInt64& minDeadline, Poco::UInt64& index, std::string& errorString);

extern "C" bool cuda_get_error(std::string& errorString);

extern "C" bool cuda_get_devices(std::vector<std::string>& devices);
extern "C" bool cuda_set_device(unsigned index);
