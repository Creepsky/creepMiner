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

#include "PlotSizes.hpp"
#include "logging/MinerLogger.hpp"

std::unordered_map<std::string, Burst::PlotSizes::HistoricalPlotSize> Burst::PlotSizes::sizes_;
Poco::Mutex Burst::PlotSizes::mutex_;

void Burst::PlotSizes::set(const std::string& plotsHash, Poco::UInt64 size)
{
	Poco::ScopedLock<Poco::Mutex> lock{ mutex_ };
	
	HistoricalPlotSize historicalSize;
	historicalSize.size = size;
	historicalSize.age = 0;

	sizes_[plotsHash] = historicalSize;
}

Poco::UInt64 Burst::PlotSizes::get(const std::string& plotsHash)
{
	Poco::ScopedLock<Poco::Mutex> lock{ mutex_ };

	auto iter = sizes_.find(plotsHash);
	
	if (iter != sizes_.end())
		return (*iter).second.size;

	return 0;
}

Poco::UInt64 Burst::PlotSizes::getTotal(Poco::UInt64 lastUpdate)
{
	Poco::ScopedLock<Poco::Mutex> lock{ mutex_ };

	Poco::UInt64 sum = 0;
	
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
