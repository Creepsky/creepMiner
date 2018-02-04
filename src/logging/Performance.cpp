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

#include "Performance.hpp"
#include <vector>
#include <iomanip>
#include <algorithm>

void Burst::Performance::reset(const std::string &id)
{
	std::lock_guard<std::mutex> lock(mutex_);
	
	auto iter = probes_.find(id);

	// if it dont exist, create it
	if (iter == probes_.end())
	{
		probes_.emplace(id, Probe());
		iter = probes_.find(id);
	}
	
	// update the time
	iter->second.currentTime = std::chrono::high_resolution_clock::now();
}

void Burst::Performance::clear()
{
	std::lock_guard<std::mutex> lock(mutex_);
	probes_.clear();
}

void Burst::Performance::takeProbe(const std::string &id)
{
	std::lock_guard<std::mutex> lock(mutex_);
	
	auto iter = probes_.find(id);

	// if it dont exist, create it
	if (iter == probes_.end())
		return;

	auto probeTime = std::chrono::high_resolution_clock::now() - iter->second.currentTime;

	// add the time
	iter->second.sumTime += probeTime;

	if (iter->second.lowestTime > probeTime || iter->second.size == 0)
		iter->second.lowestTime = probeTime;
	
	if (iter->second.highestTime < probeTime || iter->second.size == 0)
		iter->second.highestTime = probeTime;

	++iter->second.size;
}

void Burst::Performance::print(std::ostream& stream) const
{
	std::lock_guard<std::mutex> lock(mutex_);

	const auto delimiter = ';';

	stream << "name"
		<< delimiter << "avg time"
		<< delimiter << "lowest time"
		<< delimiter << "highest time"
		<< delimiter << "sum time"
		<< delimiter << "probes"
		<< std::endl;

	for (auto& probe : probes_)
		stream << probe.first
			<< std::fixed << std::setprecision(5)
			<< delimiter << probe.second.avgToSeconds()
			<< delimiter << probe.second.lowestToSeconds()
			<< delimiter << probe.second.highestToSeconds()
			<< delimiter << probe.second.sumToSeconds()
			<< delimiter << probe.second.size
			<< std::endl;
}

Burst::Performance& Burst::Performance::instance()
{
	static Performance performance;
	return performance;
}

float Burst::Performance::Probe::avgToSeconds() const
{
	if (size == 0)
		return 0.f;

	return std::chrono::duration_cast<std::chrono::duration<float>>(sumTime / size).count();
}

float Burst::Performance::Probe::lowestToSeconds() const
{
	return std::chrono::duration_cast<std::chrono::duration<float>>(lowestTime).count();
}

float Burst::Performance::Probe::highestToSeconds() const
{
	return std::chrono::duration_cast<std::chrono::duration<float>>(highestTime).count();
}

float Burst::Performance::Probe::sumToSeconds() const
{
	return std::chrono::duration_cast<std::chrono::duration<float>>(sumTime).count();
}

namespace Burst
{
	std::ostream& operator<<(std::ostream& stream, const Performance& performance)
	{
		performance.print(stream);
		return stream;
	}
}
