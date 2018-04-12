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
#include "Poco/File.h"
#include "Poco/Path.h"

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

	std::array<uint8_t, 32> target;
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


double Burst::PlotGenerator::checkPlotfileIntegrity(std::string plotPath, Miner& miner, MinerServer& server)
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
	
	const auto checkNonces = 32ull;		//number of nonces to check
	const auto checkScoops = 32ull;		//number of scoops to check per nonce
	const auto scoopSize = Settings::ScoopSize;
	const auto scoopStep = Settings::ScoopPerPlot / checkScoops;
	const auto nonceStep = (nonceCount / checkNonces);

	// drawing random numbers to decide which scoops/nonces to check
	std::vector<Poco::UInt64> scoops(checkScoops + 1);
	std::vector<Poco::UInt64> nonces(checkNonces + 1);
	for (auto i = 0; i < (checkScoops + 1); i++) scoops[i] = randInt(gen) % scoopStep;
	for (auto i = 0; i < (checkNonces + 1); i++) nonces[i] = randInt(gen) % nonceStep;

	// reading the nonces from the plot file in a parallel thread
	std::vector<char> readNonce(16 + scoopSize * checkNonces * checkScoops);

	std::thread nonceReader([&readNonce, scoops, plotPath, &miner, nonces, checkNonces, checkScoops,
		startNonce, nonceCount, staggerSize, scoopSize, scoopStep, nonceStep]
	{
		const auto nonceScoopOffset = staggerSize * scoopSize;
		for (auto scoopInterval = 0ull; scoopInterval < checkScoops; scoopInterval++)
		{
			while (miner.isProcessing()) Poco::Thread::sleep(1000);

			auto scoop = scoopInterval * scoopStep + scoops[scoopInterval];
			if (scoop >= Settings::ScoopPerPlot) scoop = Settings::ScoopPerPlot - 1;

			std::ifstream plotFile(plotPath, std::ifstream::in | std::ifstream::binary);

			for (auto nonceInterval = 0ull; nonceInterval < checkNonces; nonceInterval++)
			{
				auto nonce = startNonce + nonceInterval * nonceStep + nonces[nonceInterval];
				if (nonce >= startNonce + nonceCount) nonce = startNonce + nonceCount - 1;
				const auto nonceStaggerOffset = ((nonce - startNonce) / staggerSize) *Settings::PlotSize*staggerSize + ((nonce - startNonce) % staggerSize)*scoopSize;
				plotFile.seekg(nonceStaggerOffset + scoop * nonceScoopOffset);
				plotFile.read(readNonce.data() + nonceInterval * scoopSize + (checkNonces*scoopInterval*scoopSize), scoopSize);
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
			if (scoop >= Settings::ScoopPerPlot) scoop = Settings::ScoopPerPlot - 1;
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

void Burst::PlotGenerator::generatePlotfile(std::string plotPath, Miner& miner, MinerServer& server, std::string account, std::string sNonce, std::string nNonces)
{
	log_success(MinerLogger::general, "Starting to plot %s nonces beginning at nonce %s for accountID %s to folder %s", nNonces, sNonce, account, plotPath);
	Poco::Path plotFilePath;
	plotFilePath.parse(plotPath, Poco::Path::PATH_NATIVE);
	plotFilePath.setFileName(account + "_" + sNonce + "_" + nNonces + "_" + nNonces);
	log_success(MinerLogger::general, "Full plot path: %s", plotFilePath.toString());
	Poco::File plotFile(plotFilePath.getFileName());
	plotFile.createFile();
	plotFile.setSize(1024 * 1024);
	log_success(MinerLogger::general, "Free Space on this device: %s", std::to_string(plotFile.freeSpace())+" GB");
}

std::array<std::vector<char>, Burst::Shabal256_SSE2::HashSize> Burst::PlotGenerator::generateSse2(const Poco::UInt64 account, const Poco::UInt64 startNonce)
{
	return generate<Shabal256_SSE2, PlotGeneratorOperations1<Shabal256_SSE2>>(account, startNonce);
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
