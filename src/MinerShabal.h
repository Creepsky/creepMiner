//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#pragma once

#include "sphlib/sph_shabal.h"
#include <cstdint>

namespace Burst
{
	class Shabal256
	{
	public :
		Shabal256();
		void update(const void* data, size_t length);
		void update(const uint64_t data);
		void close(void* outData);

	private :
		sph_shabal256_context context;
	};
}
