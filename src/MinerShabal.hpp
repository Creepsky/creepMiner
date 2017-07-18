//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#pragma once

#include <cstdint>
#include <memory>
#include <Poco/ByteOrder.h>

#if USE_AVX2
#include "shabal/impl/mshabal_avx2_impl.hpp"
#elif USE_AVX
#include "shabal/impl/mshabal_avx_impl.hpp"
#elif USE_SSE4
#include "shabal/impl/mshabal_sse4_impl.h"
#else
#include "shabal/impl/sphlib_impl.hpp"
#endif


namespace Burst
{
	template <typename TAlgorithm>
	class Shabal256_Shell
	{
	public:
		static constexpr size_t HashSize = TAlgorithm::HashSize;

		Shabal256_Shell()
		{
			TAlgorithm::init(context_);
		}

		template <typename ...Args>
		void update(Args&&... args)
		{
			TAlgorithm::update(context_, std::forward<Args&&>(args)...);
		}

		void update(Poco::UInt64 singleValue)
		{
			auto result = Poco::ByteOrder::flipBytes(singleValue);
			update(&result, sizeof(Poco::UInt64));
		}

		template <typename ...Args>
		void close(Args&&... args)
		{
			TAlgorithm::close(context_, std::forward<Args&&>(args)...);
		}

	private:
		typename TAlgorithm::context_t context_;
	};

#if USE_AVX2
	using Shabal256 = Shabal256_Shell<Mshabal_avx2_Impl>;
#elif USE_AVX
	using Shabal256 = Shabal256_Shell<Mshabal_avx_Impl>;
#elif USE_SSE4
	using Shabal256 = Shabal256_Shell<Mshabal_sse4_Impl>;
#else
	using Shabal256 = Shabal256_Shell<Sphlib_Impl>;
#endif
}
