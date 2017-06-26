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
