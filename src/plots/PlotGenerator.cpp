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

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> randInt(0, nonceCount);

	log_information(MinerLogger::general, "Checking file " + plotPath + " for corruption ...");

	float totalIntegrity = 0;
	int checkNonces = 30;		//number of nonces to check
	int checkScoops = 32;		//number of scoops to check per nonce
	int noncesChecked = 0;		//counter for the case of nonceCount not devisible by checkNonces

	for (Poco::UInt64 nonceInterval = startNonce; nonceInterval < startNonce + nonceCount; nonceInterval += nonceCount / checkNonces) {

		while (miner.isProcessing()) Poco::Thread::sleep(1000);

		Poco::UInt64 nonce = nonceInterval + randInt(gen) % (nonceCount / checkNonces);
		if (nonce >= startNonce + nonceCount) nonce = startNonce + nonceCount - 1;

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

		std::ifstream plotFile(plotPath, std::ifstream::in | std::ifstream::binary);

		const Poco::UInt64 scoopSize = Settings::ScoopSize;
		Poco::UInt64 nonceStaggerOffset = ((nonce - startNonce) / staggerSize) *Settings::PlotSize*staggerSize + ((nonce - startNonce)%staggerSize)*scoopSize;
		Poco::UInt64 nonceScoopOffset = staggerSize*scoopSize;

		char readNonce[16 + Settings::PlotSize];
		char buffer[scoopSize];
		Poco::UInt64 scoopsIntact = 0;
		Poco::UInt64 scoopsChecked = 0;

		int scoopStep = Settings::ScoopPerPlot / checkScoops;
		// read scoops from nonce and compare to generated nonce
		for (int scoopInterval = 0; scoopInterval < Settings::ScoopPerPlot; scoopInterval += scoopStep)
		{
			int scoop = scoopInterval + randInt(gen) % scoopStep;
			if (scoop >= Settings::ScoopPerPlot) scoop = Settings::ScoopPerPlot - 1;
			plotFile.seekg(nonceStaggerOffset + scoop * nonceScoopOffset);
			plotFile.read(buffer, scoopSize);
			Poco::UInt64 bytesIntact = 0;
			for (int byte = 0; byte < scoopSize; byte++)
			{
				readNonce[scoop*scoopSize + byte] = buffer[byte];
				if (readNonce[scoop*scoopSize + byte] == gendata[scoop*scoopSize + byte]) bytesIntact++;
			}
			if (bytesIntact == scoopSize) scoopsIntact++;
			scoopsChecked++;
		}
		//calculate and output of integrity of this nonce
		float intact = static_cast<float>(scoopsIntact) / static_cast<float>(scoopsChecked) * 100.0f;
		log_information(MinerLogger::general, "Nonce " + std::to_string(nonce) + ": " + std::to_string(intact) + "% intact.");
		totalIntegrity += intact;
		noncesChecked++;
		plotFile.close();
	}
	log_information(MinerLogger::general, "Total Integrity: " + std::to_string(totalIntegrity / noncesChecked) + "%");

	std::string plotID = getAccountIdFromPlotFile(plotPath) + "_" + getStartNonceFromPlotFile(plotPath) + "_" + getNonceCountFromPlotFile(plotPath) + "_" + getStaggerSizeFromPlotFile(plotPath);

	//response
	Poco::JSON::Object json;
	json.set("type", "plotcheck-result");
	json.set("plotID", plotID);
	json.set("plotIntegrity", std::to_string(totalIntegrity / noncesChecked));

	server.sendToWebsockets(json);

	return totalIntegrity / noncesChecked;
}