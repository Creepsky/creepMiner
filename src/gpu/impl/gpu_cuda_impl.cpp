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

bool Burst::Gpu_Cuda_Impl::verify(const GensigData* gensig, ScoopData* gpuScoops, std::vector<ScoopData>& cpuScoops,
	size_t nonces, Poco::UInt64 nonceStart, Poco::UInt64 baseTarget, std::pair<Poco::UInt64, Poco::UInt64>& bestDeadline)
{
	std::string errorString;
	
	check(cuda_calculate_shabal_host_preallocated(gpuScoops, nonces, gensig, nonceStart, baseTarget, errorString));
	
	copyMemory(gpuScoops, cpuScoops.data(), MemoryType::Buffer, nonces, MemoryCopyDirection::ToHost);

	for (auto i = 0u; i < nonces; ++i)
	{
		auto test = cpuScoops[i];
		const auto currentDeadline_ptr = reinterpret_cast<Poco::UInt64*>(&test);
		const auto currentDeadline = *currentDeadline_ptr;

		if (i == 0 || bestDeadline.second > currentDeadline && currentDeadline > 0)
		{
			bestDeadline.first = nonceStart + i;
			bestDeadline.second = currentDeadline;
		}
	}

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
