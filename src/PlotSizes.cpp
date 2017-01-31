#include "PlotSizes.hpp"
#include <algorithm>
#include <numeric>
#include "MinerLogger.hpp"

std::unordered_map<std::string, Burst::PlotSizes::HistoricalPlotSize> Burst::PlotSizes::sizes_;
Poco::Mutex Burst::PlotSizes::mutex_;

void Burst::PlotSizes::set(const std::string& plotsHash, uint64_t size)
{
	Poco::ScopedLock<Poco::Mutex> lock{ mutex_ };
	
	HistoricalPlotSize historicalSize;
	historicalSize.size = size;
	historicalSize.age = 0;

	sizes_[plotsHash] = historicalSize;
}

uint64_t Burst::PlotSizes::get(const std::string& plotsHash)
{
	Poco::ScopedLock<Poco::Mutex> lock{ mutex_ };

	auto iter = sizes_.find(plotsHash);
	
	if (iter != sizes_.end())
		return (*iter).second.size;

	return 0;
}

uint64_t Burst::PlotSizes::getTotal(uint64_t lastUpdate)
{
	Poco::ScopedLock<Poco::Mutex> lock{ mutex_ };

	uint64_t sum = 0;
	
	for (auto& size : sizes_)
		if (lastUpdate == 0 || size.second.age <= lastUpdate)
			sum += size.second.size;

	return sum;
}

void Burst::PlotSizes::nextRound()
{
	Poco::ScopedLock<Poco::Mutex> lock{ mutex_ };

	for (auto& size : sizes_)
		size.second.age++;
}

void Burst::PlotSizes::refresh(const std::string& plotsHash)
{
	auto iter = sizes_.find(plotsHash);

	if (iter != sizes_.end())
		iter->second.age = 0;
}
