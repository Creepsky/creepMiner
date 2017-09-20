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

#include "PlotGenerator.hpp"
#include "Declarations.hpp"
#include "shabal/MinerShabal.hpp"
#include "mining/Miner.hpp"

Poco::UInt64 Burst::PlotGenerator::generateAndCheck(Poco::UInt64 account, Poco::UInt64 nonce, const Miner& miner)
{
	char final[32];
	char gendata[16 + Settings::PlotSize];

	auto xv = reinterpret_cast<char*>(&account);

	for (auto j = 0u; j <= 7; ++j)
		gendata[Settings::PlotSize + j] = xv[7 - j];

	xv = reinterpret_cast<char*>(&nonce);

	for (auto j = 0u; j <= 7; ++j)
		gendata[Settings::PlotSize + 8 + j] = xv[7 - j];

	sph_shabal256_context x;

	for (auto i = Settings::PlotSize; i > 0; i -= Settings::HashSize)
	{
		sph_shabal256_init(&x);

		auto len = Settings::PlotSize + 16 - i;

		if (len > Settings::ScoopPerPlot)
			len = Settings::ScoopPerPlot;

		sph_shabal256(&x, &gendata[i], len);
		sph_shabal256_close(&x, &gendata[i - Settings::HashSize]);
	}

	sph_shabal256_init(&x);
	sph_shabal256(&x, gendata, 16 + Settings::PlotSize);
	sph_shabal256_close(&x, final);

	// XOR with final
	for (auto i = 0; i < Settings::PlotSize; i++)
		gendata[i] ^= final[i % 32];

	std::array<uint8_t, 32> target;
	Poco::UInt64 result;

	const auto generationSignature = miner.getGensig();
	const auto scoop = miner.getScoopNum();
	const auto basetarget = miner.getBaseTarget();

	sph_shabal256_init(&x);
	sph_shabal256(&x, generationSignature.data(), Settings::HashSize);
	sph_shabal256(&x, &gendata[scoop * Settings::ScoopSize], Settings::ScoopSize);
	sph_shabal256_close(&x, target.data());

	memcpy(&result, target.data(), sizeof(Poco::UInt64));

	return result / basetarget;
}
