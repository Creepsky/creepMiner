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

#include "PlotGenerator.hpp"
#include "Declarations.hpp"
#include "shabal/MinerShabal.hpp"
#include "mining/Miner.hpp"
#include "PlotVerifier.hpp"

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

	for (auto i = Settings::PlotSize; i > 0; i -= Settings::HashSize)
	{
		Shabal256_SSE2 x;

		auto len = Settings::PlotSize + 16 - i;

		if (len > Settings::ScoopPerPlot)
			len = Settings::ScoopPerPlot;

		x.update(&gendata[i], len);
		x.close(&gendata[i - Settings::HashSize]);
	}

	Shabal256_SSE2 x;
	x.update(&gendata[0], 16 + Settings::PlotSize);
	x.close(&final[0]);

	// XOR with final
	for (size_t i = 0; i < Settings::PlotSize; i++)
		gendata[i] ^= final[i % Settings::HashSize];

	if (miner.isPoC2())
		convertToPoC2(gendata);

	std::array<uint8_t, 32> target{};
	Poco::UInt64 result;

	const auto generationSignature = miner.getGensig();
	const auto scoop = miner.getScoopNum();
	const auto basetarget = miner.getBaseTarget();

	Shabal256_SSE2 y;
	y.update(generationSignature.data(), Settings::HashSize);
	y.update(&gendata[scoop * Settings::ScoopSize], Settings::ScoopSize);
	y.close(target.data());

	memcpy(&result, target.data(), sizeof(Poco::UInt64));

	return result / basetarget;
}

std::vector<char> Burst::PlotGenerator::generateSse2(const Poco::UInt64 account, const Poco::UInt64 startNonce)
{
	const auto gendata = generate<Shabal256_SSE2, PlotGeneratorOperations1<Shabal256_SSE2>>(account, startNonce);
	return gendata[0];
}

std::array<std::vector<char>, Burst::Shabal256_AVX::HashSize> Burst::PlotGenerator::generateAvx(const Poco::UInt64 account, const Poco::UInt64 startNonce)
{
	return generate<Shabal256_AVX, PlotGeneratorOperations4<Shabal256_AVX>>(account, startNonce);
}

std::array<std::vector<char>, Burst::Shabal256_SSE4::HashSize> Burst::PlotGenerator::generateSse4(const Poco::UInt64 account, const Poco::UInt64 startNonce)
{
	return generate<Shabal256_SSE4, PlotGeneratorOperations4<Shabal256_SSE4>>(account, startNonce);
}

std::array<std::vector<char>, Burst::Shabal256_AVX2::HashSize> Burst::PlotGenerator::generateAvx2(const Poco::UInt64 account, const Poco::UInt64 startNonce)
{
	return generate<Shabal256_AVX2, PlotGeneratorOperations8<Shabal256_AVX2>>(account, startNonce);
}

Poco::UInt64 Burst::PlotGenerator::calculateDeadlineSse2(std::vector<char>& gendata,
	GensigData& generationSignature, const Poco::UInt64 scoop, const Poco::UInt64 baseTarget)
{
	std::array<std::vector<char>, 1> container = {gendata};
	return calculateDeadline<Shabal256_SSE2, PlotGeneratorOperations1<Shabal256_SSE2>>(container, generationSignature, scoop, baseTarget)[0];
}

std::array<Poco::UInt64, Burst::Shabal256_AVX::HashSize> Burst::PlotGenerator::
	calculateDeadlineAvx(std::array<std::vector<char>, Shabal256_AVX::HashSize>& gendatas,
		GensigData& generationSignature, const Poco::UInt64 scoop, const Poco::UInt64 baseTarget)
{
	return calculateDeadline<Shabal256_AVX, PlotGeneratorOperations4<Shabal256_AVX>>(gendatas, generationSignature, scoop, baseTarget);
}

std::array<Poco::UInt64, Burst::Shabal256_SSE4::HashSize> Burst::PlotGenerator::
	calculateDeadlineSse4(std::array<std::vector<char>, Shabal256_SSE4::HashSize>& gendatas,
		GensigData& generationSignature, const Poco::UInt64 scoop, const Poco::UInt64 baseTarget)
{
	return calculateDeadline<Shabal256_SSE4, PlotGeneratorOperations4<Shabal256_SSE4>>(gendatas, generationSignature, scoop, baseTarget);
}

std::array<Poco::UInt64, Burst::Shabal256_AVX2::HashSize> Burst::PlotGenerator::
	calculateDeadlineAvx2(std::array<std::vector<char>, Shabal256_AVX2::HashSize>& gendatas,
		GensigData& generationSignature, const Poco::UInt64 scoop, const Poco::UInt64 baseTarget)
{
	return calculateDeadline<Shabal256_AVX2, PlotGeneratorOperations8<Shabal256_AVX2>>(gendatas, generationSignature, scoop, baseTarget);
}

void Burst::PlotGenerator::convertToPoC2(char* gendata)
{
	std::array<char, Settings::HashSize> buffer{};
	auto indexMirror = Settings::HashSize - Settings::ScoopSize;

	for (size_t i = 0; i < Settings::PlotSize / 2; i += Settings::ScoopSize)
	{
		const auto scoop = &gendata[i + Settings::HashSize];
		const auto mirrorScoop = &gendata[indexMirror + Settings::HashSize];

		memcpy(buffer.data(), scoop, Settings::HashSize);
		memcpy(scoop, mirrorScoop, Settings::HashSize);
		memcpy(mirrorScoop, buffer.data(), Settings::HashSize);

		indexMirror -= Settings::ScoopSize;
	}
}
