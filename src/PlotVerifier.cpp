#include "PlotVerifier.hpp"
#include "MinerShabal.hpp"
#include "Miner.hpp"

Burst::PlotVerifier::PlotVerifier(Miner& miner, std::vector<ScoopData>&& buffer, uint64_t nonceRead, const GensigData& gensig,
	uint64_t nonceStart, const std::string& inputPath, uint64_t accountId)
	: Task("PlotVerifier")
	, miner_{ &miner }
	, buffer_{ std::move(buffer) }
	, nonceRead_{ nonceRead }
	, gensig_{ &gensig }
	, nonceStart_{ nonceStart }
	, inputPath_{ &inputPath }
	, accountId_{ accountId }
{}

void Burst::PlotVerifier::runTask()
{
	Shabal256 hash;

	for (size_t i = 0; i < buffer_.size() && !isCancelled(); i++)
	{
		HashData target;
		auto test = buffer_.data() + i;
		hash.update(gensig_->data(), Settings::HashSize);
		hash.update(test, Settings::ScoopSize);
		hash.close(&target[0]);

		uint64_t targetResult = 0;
		memcpy(&targetResult, &target[0], sizeof(decltype(targetResult)));
		auto deadline = targetResult / miner_->getBaseTarget();

		auto nonceNum = nonceStart_ + nonceRead_ + i;
		miner_->submitNonce(nonceNum, accountId_, deadline, *inputPath_);
		//++nonceRead_;
	}
}
