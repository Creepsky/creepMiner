#pragma once

#include <memory>
#include <Poco/Types.h>

namespace Burst
{
	template <typename TAlgorithm>
	struct Gpu_Algorithm_Shell
	{
		template <typename TGpu_Shell, typename ...TArgs>
		static std::pair<Poco::UInt64, Poco::UInt64> run(TArgs&&... args)
		{
			return TAlgorithm::run(std::forward<TArgs&&>(args)...);
		}
	};
}
