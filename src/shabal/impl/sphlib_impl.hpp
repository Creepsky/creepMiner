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
