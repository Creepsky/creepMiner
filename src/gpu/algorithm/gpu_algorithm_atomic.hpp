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
#include "gpu/gpu_shell.hpp"
#include "logging/Message.hpp"

namespace Burst
{
	struct Gpu_Algorithm_Atomic
	{
		template <typename TGpu_Impl>
		static bool run(std::vector<ScoopData>& scoops,
			const GensigData& gensig,
			Poco::UInt64 nonceStart, Poco::UInt64 baseTarget,
			std::pair<Poco::UInt64, Poco::UInt64>& bestDeadline)
		{
			using shell = Gpu_Shell<TGpu_Impl>;

			auto ok = true;
			ScoopData* gpuScoops;
			GensigData* gpuGensig;
			Poco::UInt64* gpuDeadlines;
			std::string errorString;
			const auto nonces = scoops.size();
			Poco::UInt64 minDeadline;
			Poco::UInt64 minDeadlineIndex;

			// allocate the memory for the gpu
			ok = ok && shell::allocateMemory(reinterpret_cast<void**>(&gpuScoops), MemoryType::Buffer, nonces);
			ok = ok && shell::allocateMemory(reinterpret_cast<void**>(&gpuGensig), MemoryType::Gensig, 1);
			ok = ok && shell::allocateMemory(reinterpret_cast<void**>(&gpuDeadlines), MemoryType::Bytes, nonces * sizeof(Poco::UInt64));

			// copy the memory from RAM to gpu
			ok = shell::copyMemory(scoops.data(), gpuScoops, MemoryType::Buffer, nonces, MemoryCopyDirection::ToDevice);
			ok = shell::copyMemory(&gensig, gpuGensig, MemoryType::Gensig, 1, MemoryCopyDirection::ToDevice);

			// calculate the deadlines on gpu
			ok = ok && shell::verify(gpuGensig, gpuScoops, gpuDeadlines, nonces, nonceStart, baseTarget);

			// get the best deadline on gpu
			ok = ok && shell::getMinDeadline(gpuDeadlines, nonces, minDeadline, minDeadlineIndex);

			// fetch the last error if there is one
			ok = !shell::getError(errorString);

			if (ok)
			{
				bestDeadline.first = nonceStart + minDeadlineIndex;
				bestDeadline.second = minDeadline;
			}
			else
			{
				// print the error
				log_error(MinerLogger::plotVerifier, "Error while verifying a plot file!\n\tError: %s", errorString);
			}

			// give the memory on gpu free
			shell::freeMemory(gpuScoops);
			shell::freeMemory(gpuGensig);
			shell::freeMemory(gpuDeadlines);
			
			return ok;
		}
	};
}
