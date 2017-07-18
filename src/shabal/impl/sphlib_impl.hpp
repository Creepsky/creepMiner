#pragma once

#include "shabal/sphlib/sph_shabal.h"
#include <Poco/Types.h>

namespace Burst
{
	struct Sphlib_Impl
	{
		static constexpr size_t HashSize = 1;

		using context_t = sph_shabal256_context;

		static void init(context_t& context)
		{
			sph_shabal256_init(&context);
		}

		static void update(context_t& context, const void* data, size_t length)
		{
			sph_shabal256(&context, data, length);
		}

		static void close(context_t& context, void* out)
		{
			sph_shabal256_close(&context, out);
		}
	};
}
