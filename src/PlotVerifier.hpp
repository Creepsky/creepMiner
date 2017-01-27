#pragma once

#include <Poco/Task.h>
#include <vector>
#include "Declarations.hpp"

namespace Burst
{
	class Miner;

	class PlotVerifier : public Poco::Task
	{
	public:
		PlotVerifier(Miner &miner, std::vector<ScoopData>&& buffer, uint64_t nonceRead, const GensigData& gensig,
			uint64_t nonceStart, const std::string& inputPath, uint64_t accountId);
		void runTask() override;

	private:
		Miner* miner_;
		std::vector<ScoopData> buffer_;
		uint64_t nonceRead_;
		const GensigData* gensig_;
		uint64_t nonceStart_;
		const std::string* inputPath_;
		uint64_t accountId_;
	};
}
