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

#include "shabal/mshabal/mshabal.h"

namespace Burst
{
	struct Mshabal_avx2_Impl
	{
		static constexpr size_t HashSize = 8;

		using context_t = mshabal256_context;

		static void init(context_t& context)
		{
			avx2_mshabal_init(&context, 256);
		}

		static void update(context_t& context, const void* data, size_t length)
		{
			update(context, data, data, data, data, data, data, data, data, length);
		}

		static void update(context_t& context,
		                   const void* data1, const void* data2, const void* data3, const void* data4,
		                   const void* data5, const void* data6, const void* data7, const void* data8,
		                   size_t length)
		{
			avx2_mshabal(&context, data1, data2, data3, data4, data5, data6, data7, data8, length);
		}

		static void close(context_t& context, void* output)
		{
			avx2_mshabal_close(&context, 0, 0, 0, 0, 0, 0, 0, 0, 0, output,
			                   nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		}

		static void close(context_t& context,
		                  void* out1, void* out2, void* out3, void* out4,
		                  void* out5, void* out6, void* out7, void* out8)
		{
			avx2_mshabal_close(&context, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			                   out1, out2, out3, out4, out5, out6, out7, out8);
		}
	};
}
