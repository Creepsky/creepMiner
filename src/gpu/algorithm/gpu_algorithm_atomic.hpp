#pragma once

#include "Declarations.hpp"
#include "gpu/gpu_shell.hpp"
#include "logging/Message.hpp"

// this include is only a workaround for compilers without make_unique
#include "MinerUtil.hpp"

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
			ScoopData* gpu_scoops;
			GensigData* gpu_gensig;
			std::string errorString;
			auto nonces = scoops.size();

			// allocate the memory for the gpu
			ok = ok && shell::allocateMemory((void**)&gpu_scoops, MemoryType::Buffer, nonces);
			ok = ok && shell::allocateMemory((void**)&gpu_gensig, MemoryType::Gensig, 1);

			// copy the memory from RAM to gpu
			ok = shell::copyMemory(scoops.data(), gpu_scoops, MemoryType::Buffer, nonces, MemoryCopyDirection::ToDevice);
			ok = shell::copyMemory(&gensig, gpu_gensig, MemoryType::Gensig, 1, MemoryCopyDirection::ToDevice);

			// calculate the deadlines on gpu
			ok = ok && shell::verify(gpu_gensig, gpu_scoops, scoops, nonces, nonceStart, baseTarget, bestDeadline);

			// fetch the last error if there is one
			ok = !shell::getError(errorString);

			if (!ok)
				// print the error
				log_error(MinerLogger::plotVerifier, "Error while verifying a plot file!\n\tError: %s", errorString);

			// give the memory on gpu free
			shell::freeMemory(gpu_scoops);
			shell::freeMemory(gpu_gensig);
			
			return ok;
		}
	};
}
