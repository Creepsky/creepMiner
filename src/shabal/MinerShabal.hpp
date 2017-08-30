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

#include <cstdint>
#include <memory>
#include <Poco/ByteOrder.h>

#if USE_AVX2
#include "shabal/impl/mshabal_avx2_impl.hpp"
#elif USE_AVX
#include "shabal/impl/mshabal_avx_impl.hpp"
#elif USE_SSE4
#include "shabal/impl/mshabal_sse4_impl.hpp"
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
