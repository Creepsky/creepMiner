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
#include "MinerUtil.hpp"
#include <fstream>
#include <random>
#include "webserver/MinerServer.hpp"
#include <future>
#include <thread>

Poco::UInt64 Burst::PlotGenerator::generateAndCheck(Poco::UInt64 account, Poco::UInt64 nonce, const Miner& miner)
{
	char final[32];
	char gendata[16 + Settings::plotSize];

	auto xv = reinterpret_cast<char*>(&account);

	for (auto j = 0u; j <= 7; ++j)
		gendata[Settings::plotSize + j] = xv[7 - j];

	xv = reinterpret_cast<char*>(&nonce);

	for (auto j = 0u; j <= 7; ++j)
		gendata[Settings::plotSize + 8 + j] = xv[7 - j];

	for (auto i = Settings::plotSize; i > 0; i -= Settings::hashSize)
	{
		Shabal256Sse2 x;

		auto len = Settings::plotSize + 16 - i;

		if (len > Settings::scoopPerPlot)
			len = Settings::scoopPerPlot;

		x.update(&gendata[i], len);
		x.close(&gendata[i - Settings::hashSize]);
	}

	Shabal256Sse2 x;
	x.update(&gendata[0], 16 + Settings::plotSize);
	x.close(&final[0]);

	// XOR with final
	for (size_t i = 0; i < Settings::plotSize; i++)
		gendata[i] ^= final[i % Settings::hashSize];

	std::array<uint8_t, 32> target{};
	Poco::UInt64 result;

	const auto generationSignature = miner.getGensig();
	const auto scoop = miner.getScoopNum();
	const auto basetarget = miner.getBaseTarget();

	Shabal256Sse2 y;
	y.update(generationSignature.data(), Settings::hashSize);
	y.update(&gendata[scoop * Settings::scoopSize], Settings::scoopSize);
	y.close(target.data());

	memcpy(&result, target.data(), sizeof(Poco::UInt64));

	return result / basetarget;
}


double Burst::PlotGenerator::checkPlotfileIntegrity(const std::string& plotPath, Miner& miner, MinerServer& server)
{
	const auto account = Poco::NumberParser::parseUnsigned64(getAccountIdFromPlotFile(plotPath));
	const auto startNonce = Poco::NumberParser::parseUnsigned64(getStartNonceFromPlotFile(plotPath));
	const auto nonceCount = Poco::NumberParser::parseUnsigned64(getNonceCountFromPlotFile(plotPath));
	const auto staggerSize = Poco::NumberParser::parseUnsigned64(getStaggerSizeFromPlotFile(plotPath));

	//set up random number generator
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<Poco::UInt64> randInt(0, nonceCount);

	log_system(MinerLogger::general, "Validating the integrity of file " + plotPath + " ...");

	const auto checkNonces = 32ull; //number of nonces to check
	const auto checkScoops = 32ull; //number of scoops to check per nonce
	const auto scoopSize = Settings::scoopSize;
	const auto scoopStep = Settings::scoopPerPlot / checkScoops;
	const auto nonceStep = (nonceCount / checkNonces);

	// drawing random numbers to decide which scoops/nonces to check
	std::vector<Poco::UInt64> scoops(checkScoops + 1);
	std::vector<Poco::UInt64> nonces(checkNonces + 1);
	for (Poco::UInt64 i = 0; i < (checkScoops + 1); i++) scoops[i] = randInt(gen) % scoopStep;
	for (Poco::UInt64 i = 0; i < (checkNonces + 1); i++) nonces[i] = randInt(gen) % nonceStep;

	// reading the nonces from the plot file in a parallel thread
	std::vector<char> readNonce(16 + scoopSize * checkNonces * checkScoops);

	std::thread nonceReader([&]
	{
		const auto nonceScoopOffset = staggerSize * scoopSize;
		for (auto scoopInterval = 0ull; scoopInterval < checkScoops; scoopInterval++)
		{
			while (miner.isProcessing()) Poco::Thread::sleep(1000);

			auto scoop = scoopInterval * scoopStep + scoops[scoopInterval];
			if (scoop >= Settings::scoopPerPlot) scoop = Settings::scoopPerPlot - 1;

			std::ifstream plotFile(plotPath, std::ifstream::in | std::ifstream::binary);

			for (auto nonceInterval = 0ull; nonceInterval < checkNonces; nonceInterval++)
			{
				auto nonce = startNonce + nonceInterval * nonceStep + nonces[nonceInterval];
				if (nonce >= startNonce + nonceCount) nonce = startNonce + nonceCount - 1;
				const auto nonceStaggerOffset = ((nonce - startNonce) / staggerSize) * Settings::plotSize * staggerSize + ((nonce -
					startNonce) % staggerSize) * scoopSize;
				plotFile.seekg(nonceStaggerOffset + scoop * nonceScoopOffset);
				plotFile.read(readNonce.data() + nonceInterval * scoopSize + (checkNonces * scoopInterval * scoopSize), scoopSize);
			}
			plotFile.close();
		}
	});

	//Generating the Nonces to compare the scoops with what we have read from the file
	std::array<std::vector<char>, checkNonces> genData;

	for (auto nonceInterval = 0ull; nonceInterval < checkNonces; nonceInterval++)
	{
		auto nonce = startNonce + nonceInterval * nonceStep + nonces[nonceInterval];
		if (nonce >= startNonce + nonceCount) nonce = startNonce + nonceCount - 1;
		genData[nonceInterval] = generateSse2(account, nonce)[0];
	}

	//waiting for read thread to finish
	nonceReader.join();

	//Comparing the read nonces with the generated ones
	auto totalIntegrity = 0.0;
	auto noncesChecked = 0;

	for (auto nonceInterval = 0ull; nonceInterval < checkNonces; nonceInterval++)
	{
		auto nonce = startNonce + nonceInterval * nonceStep + nonces[nonceInterval];
		if (nonce >= startNonce + nonceCount) nonce = startNonce + nonceCount - 1;
		auto scoopsIntact = 0ull;
		auto scoopsChecked = 0ull;

		for (auto scoopInterval = 0ull; scoopInterval < checkScoops; scoopInterval++)
		{
			auto scoop = scoopInterval * scoopStep + scoops[scoopInterval];
			if (scoop >= Settings::scoopPerPlot) scoop = Settings::scoopPerPlot - 1;
			const auto scoopPos = scoop * scoopSize;
			const auto nonceScoopPos = nonceInterval * scoopSize + (checkNonces*scoopInterval*scoopSize);
			auto bytes = 0;
			for (auto i = 0ull; i < scoopSize; i++)
				if (genData[nonceInterval][scoopPos + i] == readNonce[nonceScoopPos + i]) bytes++;
			if (bytes == 64)
				scoopsIntact++;
			scoopsChecked++;
		}
		const auto intact = 100.0 * scoopsIntact / scoopsChecked;
		//log_information(MinerLogger::general, "Nonce %s: %s% Integrity.", std::to_string(nonce), std::to_string(intact));
		totalIntegrity += intact;
		noncesChecked++;
	}

	if(totalIntegrity / noncesChecked < 100.0)
		log_error(MinerLogger::general, "Total Integrity of %s: %0.3f%%", plotPath, totalIntegrity / noncesChecked);
	else
		log_success(MinerLogger::general, "Total Integrity of %s: %0.3f%%", plotPath, totalIntegrity / noncesChecked);

	const auto plotId = getAccountIdFromPlotFile(plotPath) + "_" + getStartNonceFromPlotFile(plotPath) + "_" + getNonceCountFromPlotFile(plotPath) + "_" + getStaggerSizeFromPlotFile(plotPath);

	//response to websockets
	Poco::JSON::Object json;
	json.set("type", "plotcheck-result");
	json.set("plotID", plotId);
	json.set("plotIntegrity", std::to_string(totalIntegrity / noncesChecked));

	server.sendToWebsockets(json);

	return totalIntegrity / noncesChecked;
}

std::array<std::vector<char>, Burst::Shabal256Sse2::HashSize> Burst::PlotGenerator::generateSse2(const Poco::UInt64 account, const Poco::UInt64 startNonce)
{
	return generate<Shabal256Sse2, PlotGeneratorOperations1<Shabal256Sse2>>(account, startNonce);
}

std::array<std::vector<char>, Burst::Shabal256Avx::HashSize> Burst::PlotGenerator::generateAvx(const Poco::UInt64 account, const Poco::UInt64 startNonce)
{
	return generate<Shabal256Avx, PlotGeneratorOperations4<Shabal256Avx>>(account, startNonce);
}

std::array<std::vector<char>, Burst::Shabal256Sse4::HashSize> Burst::PlotGenerator::generateSse4(const Poco::UInt64 account, const Poco::UInt64 startNonce)
{
	return generate<Shabal256Sse4, PlotGeneratorOperations4<Shabal256Sse4>>(account, startNonce);
}

std::array<std::vector<char>, Burst::Shabal256Avx2::HashSize> Burst::PlotGenerator::generateAvx2(const Poco::UInt64 account, const Poco::UInt64 startNonce)
{
	return generate<Shabal256Avx2, PlotGeneratorOperations8<Shabal256Avx2>>(account, startNonce);
}

Poco::UInt64 Burst::PlotGenerator::calculateDeadlineSse2(std::vector<char>& gendata,
	GensigData& generationSignature, const Poco::UInt64 scoop, const Poco::UInt64 baseTarget)
{
	std::array<std::vector<char>, 1> container = {gendata};
	return calculateDeadline<Shabal256Sse2, PlotGeneratorOperations1<Shabal256Sse2>>(container, generationSignature, scoop, baseTarget)[0];
}

std::array<Poco::UInt64, Burst::Shabal256Avx::HashSize> Burst::PlotGenerator::
	calculateDeadlineAvx(std::array<std::vector<char>, Shabal256Avx::HashSize>& gendatas,
		GensigData& generationSignature, const Poco::UInt64 scoop, const Poco::UInt64 baseTarget)
{
	return calculateDeadline<Shabal256Avx, PlotGeneratorOperations4<Shabal256Avx>>(gendatas, generationSignature, scoop, baseTarget);
}

std::array<Poco::UInt64, Burst::Shabal256Sse4::HashSize> Burst::PlotGenerator::
	calculateDeadlineSse4(std::array<std::vector<char>, Shabal256Sse4::HashSize>& gendatas,
		GensigData& generationSignature, const Poco::UInt64 scoop, const Poco::UInt64 baseTarget)
{
	return calculateDeadline<Shabal256Sse4, PlotGeneratorOperations4<Shabal256Sse4>>(gendatas, generationSignature, scoop, baseTarget);
}

std::array<Poco::UInt64, Burst::Shabal256Avx2::HashSize> Burst::PlotGenerator::
	calculateDeadlineAvx2(std::array<std::vector<char>, Shabal256Avx2::HashSize>& gendatas,
		GensigData& generationSignature, const Poco::UInt64 scoop, const Poco::UInt64 baseTarget)
{
	return calculateDeadline<Shabal256Avx2, PlotGeneratorOperations8<Shabal256Avx2>>(gendatas, generationSignature, scoop, baseTarget);
}
