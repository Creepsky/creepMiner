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
	Poco::UInt64 account = Poco::NumberParser::parseUnsigned64(getAccountIdFromPlotFile(plotPath));
	Poco::UInt64 startNonce = Poco::NumberParser::parseUnsigned64(getStartNonceFromPlotFile(plotPath));
	Poco::UInt64 nonceCount = Poco::NumberParser::parseUnsigned64(getNonceCountFromPlotFile(plotPath));
	Poco::UInt64 staggerSize = Poco::NumberParser::parseUnsigned64(getStaggerSizeFromPlotFile(plotPath));

	//set up random number generator
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> randInt(0, nonceCount);

	log_system(MinerLogger::general, "Validating the integrity of file " + plotPath + " ...");
	
	const Poco::UInt64 checkNonces = 32;		//number of nonces to check
	const Poco::UInt64  checkScoops = 32;		//number of scoops to check per nonce
	float totalIntegrity = 0;
	auto noncesChecked = 0;		


	const Poco::UInt64 scoopSize = Settings::ScoopSize;
	auto scoopStep = Settings::ScoopPerPlot / checkScoops;
	auto nonceStep = (nonceCount / checkNonces);
	Poco::UInt64 nonceScoopOffset = staggerSize * scoopSize;

	std::vector<Poco::UInt64> scoops(checkScoops + 1);
	std::vector<Poco::UInt64> nonces(checkNonces + 1);
	for (auto i = 0; i < (checkScoops + 1); i++) scoops[i] = randInt(gen) % scoopStep;
	for (auto i = 0; i < (checkNonces + 1); i++) nonces[i] = randInt(gen) % nonceStep;

	std::vector<char> readNonce(16 + scoopSize * checkNonces * checkScoops);

	//Reading the (checkScoops) random scoops of (checkNonces) random nonces from the file

	//starting thread to read the nonces from the file
	PlotCheckReader PCReader(scoops, nonces, readNonce, miner, plotPath, checkNonces, checkScoops);
	Poco::Thread readerThread;
	readerThread.start(PCReader);

	//Generating the Nonces to compare the scoops with what we have read from the file
	
	std::array<std::vector<char>, checkNonces> genData;

	for (Poco::UInt64 nonceInterval = 0; nonceInterval < checkNonces; nonceInterval++)
	{
		Poco::UInt64 nonce = startNonce + nonceInterval * nonceStep + nonces[nonceInterval];
		if (nonce >= startNonce + nonceCount) nonce = startNonce + nonceCount - 1;
		genData[nonceInterval] = generateSse2(account, nonce)[0];
	}

	//waiting for read thread to finish
	readerThread.join();

	//Comparing the read nonces with the generated ones
	for (Poco::UInt64 nonceInterval = 0; nonceInterval < checkNonces; nonceInterval++)
	{
		Poco::UInt64 nonce = startNonce + nonceInterval * nonceStep + nonces[nonceInterval];
		if (nonce >= startNonce + nonceCount) nonce = startNonce + nonceCount - 1;
		Poco::UInt64 scoopsIntact = 0;
		Poco::UInt64 scoopsChecked = 0;

		for (Poco::UInt64 scoopInterval = 0; scoopInterval < checkScoops; scoopInterval++)
		{
			int scoop = scoopInterval * scoopStep + scoops[scoopInterval];
			if (scoop >= Settings::ScoopPerPlot) scoop = Settings::ScoopPerPlot - 1;
			auto scoopPos = scoop * scoopSize;
			auto nonceScoopPos = nonceInterval * scoopSize + (checkNonces*scoopInterval*scoopSize);
			auto bytes = 0;
			for (auto i = 0; i < scoopSize; i++)
				if (genData[nonceInterval][scoopPos + i] == readNonce[nonceScoopPos + i]) bytes++;
			if (bytes == 64)
				scoopsIntact++;
			scoopsChecked++;
		}
		float intact = 100.0f * scoopsIntact / scoopsChecked;
		//log_information(MinerLogger::general, "Nonce %s: %s% Integrity.", std::to_string(nonce), std::to_string(intact));
		totalIntegrity += intact;
		noncesChecked++;
	}

	if(totalIntegrity / noncesChecked < 100.0)
		log_error(MinerLogger::general, "Total Integrity: " + std::to_string(totalIntegrity / noncesChecked) + "%");
	else
		log_success(MinerLogger::general, "Total Integrity: " + std::to_string(totalIntegrity / noncesChecked) + "%");

	std::string plotID = getAccountIdFromPlotFile(plotPath) + "_" + getStartNonceFromPlotFile(plotPath) + "_" + getNonceCountFromPlotFile(plotPath) + "_" + getStaggerSizeFromPlotFile(plotPath);

	//response
	Poco::JSON::Object json;
	json.set("type", "plotcheck-result");
	json.set("plotID", plotID);
	json.set("plotIntegrity", std::to_string(totalIntegrity / noncesChecked));

	server.sendToWebsockets(json);

	return totalIntegrity / noncesChecked;
}

void Burst::PlotCheckReader::run()
{
	for (auto scoopInterval = 0; scoopInterval < checkScoops; scoopInterval++)
	{
		Poco::UInt64 account = Poco::NumberParser::parseUnsigned64(getAccountIdFromPlotFile(plotPath));
		Poco::UInt64 startNonce = Poco::NumberParser::parseUnsigned64(getStartNonceFromPlotFile(plotPath));
		Poco::UInt64 nonceCount = Poco::NumberParser::parseUnsigned64(getNonceCountFromPlotFile(plotPath));
		Poco::UInt64 staggerSize = Poco::NumberParser::parseUnsigned64(getStaggerSizeFromPlotFile(plotPath));

		const Poco::UInt64 scoopSize = Settings::ScoopSize;
		auto scoopStep = Settings::ScoopPerPlot / checkScoops;
		auto nonceStep = (nonceCount / checkNonces);
		Poco::UInt64 nonceScoopOffset = staggerSize * scoopSize;

		while (miner->isProcessing()) Poco::Thread::sleep(1000);

		int scoop = scoopInterval * scoopStep + scoops[scoopInterval];
		if (scoop >= Settings::ScoopPerPlot) scoop = Settings::ScoopPerPlot - 1;

		std::ifstream plotFile(plotPath, std::ifstream::in | std::ifstream::binary);

		// read scoops from nonce and compare to generated nonce
		for (Poco::UInt64 nonceInterval = 0; nonceInterval < checkNonces; nonceInterval++)
		{
			Poco::UInt64 nonce = startNonce + nonceInterval * nonceStep + nonces[nonceInterval];
			if (nonce >= startNonce + nonceCount) nonce = startNonce + nonceCount - 1;
			Poco::UInt64 nonceStaggerOffset = ((nonce - startNonce) / staggerSize) *Settings::PlotSize*staggerSize + ((nonce - startNonce) % staggerSize)*scoopSize;
			plotFile.seekg(nonceStaggerOffset + scoop * nonceScoopOffset);
			plotFile.read(readNonce->data() + nonceInterval * scoopSize + (checkNonces*scoopInterval*scoopSize), scoopSize);
		}
		plotFile.close();
	}
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
