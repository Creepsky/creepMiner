#include "PlotGenerator.hpp"
#include "Declarations.hpp"
#include "MinerShabal.hpp"
#include "MinerUtil.hpp"
#include "NonceSubmitter.hpp"
#include "PlotReader.hpp"
#include <random>
#include "MinerLogger.hpp"
#include <Poco/Net/HTTPClientSession.h>
#include "Miner.hpp"

Burst::PlotGenerator::PlotGenerator(uint64_t account, uint64_t staggerSize, uint64_t startNonce, uint64_t nonces, void* output)
	: account_{account}, staggerSize_{staggerSize}, startNonce_{startNonce}, nonces_{nonces}, output_{output}
{}

void Burst::PlotGenerator::run()
{
	for (auto i = 0u; i < nonces_; ++i)
		generate(account_, staggerSize_, startNonce_, i, output_);
}

void* Burst::PlotGenerator::getOutput() const
{
	return output_;
}

void* Burst::PlotGenerator::generate(uint64_t account, uint64_t staggerSize, uint64_t startNonce, uint64_t cachePos, void* output)
{
	char final[32];
	char gendata[16 + Settings::PlotSize];
	Shabal256 shabal;
	//auto mem = calloc(Settings::PlotSize, staggerSize);
	auto cache = static_cast<char*>(output);

	auto *xv = reinterpret_cast<char*>(&account);

	gendata[Settings::PlotSize] = xv[7]; gendata[Settings::PlotSize + 1] = xv[6]; gendata[Settings::PlotSize + 2] = xv[5]; gendata[Settings::PlotSize + 3] = xv[4];
	gendata[Settings::PlotSize + 4] = xv[3]; gendata[Settings::PlotSize + 5] = xv[2]; gendata[Settings::PlotSize + 6] = xv[1]; gendata[Settings::PlotSize + 7] = xv[0];

	xv = reinterpret_cast<char*>(&startNonce);

	gendata[Settings::PlotSize + 8] = xv[7]; gendata[Settings::PlotSize + 9] = xv[6]; gendata[Settings::PlotSize + 10] = xv[5]; gendata[Settings::PlotSize + 11] = xv[4];
	gendata[Settings::PlotSize + 12] = xv[3]; gendata[Settings::PlotSize + 13] = xv[2]; gendata[Settings::PlotSize + 14] = xv[1]; gendata[Settings::PlotSize + 15] = xv[0];
	
	for (auto i = Settings::PlotSize; i > 0; i -= Settings::HashSize)
	{
		shabal = {};

		auto len = Settings::PlotSize + 16 - i;

		if (len > Settings::ScoopPerPlot)
			len = Settings::ScoopPerPlot;

		shabal.update(&gendata[i], len);
		shabal.close(&gendata[i - Settings::HashSize]);
	}

	shabal = {};
	shabal.update(gendata, 16 + Settings::PlotSize);
	shabal.close(&final);

	// XOR with final
	for (auto i = 0u; i < Settings::PlotSize; i++)
		gendata[i] ^= (final[i % 32]);

	// Sort them:
	for (auto i = 0u; i < Settings::PlotSize; i += 64)
		memmove(&cache[cachePos * 64 + static_cast<unsigned long long>(i) * staggerSize], &gendata[i], 64);
	
	return cache;
}

Burst::RandomNonceGenerator::RandomNonceGenerator(Miner& miner, uint64_t account, uint64_t staggerSize, uint64_t randomNonces)
	: Task("RandomNonceGenerator"), miner_{&miner}, account_{account}, staggerSize_{staggerSize}, randomNonces_{randomNonces}
{}

struct membuf : std::streambuf
{
	membuf(char* begin, char* end) {
		this->setg(begin, begin, end);
	}
};

void Burst::RandomNonceGenerator::runTask()
{
	std::default_random_engine rd;
	std::mt19937 mt(rd());
	std::uniform_int_distribution<uint64_t> dist;
	
	auto mem = calloc(Settings::PlotSize, static_cast<size_t>(staggerSize_));
	auto gensig = miner_->getGensig();

	std::vector<ScoopData> buffer;
	buffer.resize(1);

	auto scoopData = reinterpret_cast<char*>(&buffer[0]);

	for (auto i = 0u; (i < randomNonces_ || randomNonces_ == 0) && !isCancelled(); ++i)
	{
		//auto startNonce = dist(mt);

		//PlotGenerator plotGenerator{account_, staggerSize_, startNonce, 1, mem};
		//plotGenerator.run();

		////PlotGenerator::generate(account_, staggerSize_, startNonce, mem);

		//auto begin = reinterpret_cast<char*>(mem);
		////auto end = begin + Settings::PlotSize * staggerSize_;
		//
		//Shabal256 hash;
		//std::istringstream{begin}.read(scoopData, staggerSize_);

		//HashData target;
		//auto test = buffer.data();
		//hash.update(gensig.data(), Settings::HashSize);
		//hash.update(test, Settings::ScoopSize);
		//hash.close(&target[0]);

		//uint64_t targetResult = 0;
		//memcpy(&targetResult, &target[0], sizeof(decltype(targetResult)));
		//auto deadline = targetResult / miner_->getBaseTarget();

		//auto nonceNum = startNonce;
		//miner_->submitNonce(nonceNum, account_, deadline, "");

		//PlotReader plotReader{ *miner_, std::make_unique<std::istringstream>(begin), account_, startNonce,
			//1, staggerSize_ };
		//plotReader.runTask();
	}

	log_debug(MinerLogger::general, "Random nonce generator finished...");
	free(mem);
}
