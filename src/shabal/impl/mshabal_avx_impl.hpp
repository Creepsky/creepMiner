// ==========================================================================
// 
// creepMiner - Burstcoin cryptocurrency CPU and GPU miner
// Copyright (C)  2016-2018 Creepsky (creepsky@gmail.com)
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

#include "shabal/mshabal/mshabal.h"

namespace Burst
{
	struct Mshabal_avx_Impl
	{
		static constexpr size_t HashSize = 4;

		using context_t = mshabal_context;

		static void init(context_t& context)
		{
			avx1_mshabal_init(&context, 256);
		}

		static void update(context_t& context, const void* data, size_t length)
		{
			update(context, data, data, data, data, length);
		}

		static void update(context_t& context,
		                   const void* data1, const void* data2, const void* data3, const void* data4,
		                   size_t length)
		{
			avx1_mshabal(&context, data1, data2, data3, data4, length);
		}

		static void close(context_t& context, void* output)
		{
			avx1_mshabal_close(&context, 0, 0, 0, 0, 0, output, nullptr, nullptr, nullptr);
		}

		static void close(context_t& context,
			void* out1, void* out2, void* out3, void* out4)
		{
			avx1_mshabal_close(&context, 0, 0, 0, 0, 0, out1, out2, out3, out4);
		}
	};
}

#ifndef USE_AVX
inline void avx1_mshabal_init(mshabal_context* sc, unsigned out_size) {}

inline void avx1_mshabal(mshabal_context* sc, const void* data0, const void* data1, const void* data2, const void* data3,
                  size_t len) {}

inline void avx1_mshabal_close(mshabal_context* sc, unsigned ub0, unsigned ub1, unsigned ub2, unsigned ub3, unsigned n,
                        void* dst0, void* dst1, void* dst2, void* dst3) {}
#endif
