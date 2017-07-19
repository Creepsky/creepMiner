// ==========================================================================
//
// creepMiner - Burstcoin cryptocurrency CPU and GPU miner
// Copyright (C)  2016-2017 Creepsky (creepsky@gmail.com)
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

	// add the time
	iter->second.sumTime += std::chrono::high_resolution_clock::now() - iter->second.currentTime;
	++iter->second.size;
}

void Burst::Performance::print(std::ostream& stream) const
{
	std::lock_guard<std::mutex> lock(mutex_);

	stream << std::string(50, '-') << std::endl;

	for (auto& probe : probes_)
		stream << probe.first << ": "
			<< std::fixed << std::setprecision(5) << probe.second.toSeconds()
			<< " (" << probe.second.sumToSeconds() << ") " << " seconds, "
			<< probe.second.size << " probe(s)"
			<< std::endl;

	// create top 5 list
	using ProbeEntry_t = std::pair<const std::string*, const Probe*>;

	std::vector<ProbeEntry_t> probeToplist;
	probeToplist.reserve(probes_.size());

	for (auto& probe : probes_)
		probeToplist.emplace_back(&probe.first, &probe.second);

	std::sort(probeToplist.begin(), probeToplist.end(), [](const ProbeEntry_t& lhs, const ProbeEntry_t& rhs)
	{		
		return lhs.second->sumTime > rhs.second->sumTime;
	});

	stream << "toplist" << std::endl;

	for (auto i = 0u; i < probeToplist.size() && i < 10; ++i)
	{
		stream << i + 1 << ": "
			<< *probeToplist[i].first << " "
			<< std::fixed << std::setprecision(5)
			<< probeToplist[i].second->toSeconds();
		
		if (probeToplist[i].second->size > 1)
			stream << " (" << probeToplist[i].second->sumToSeconds() << ")";

		stream << std::endl;
	}

	stream << std::string(50, '-') << std::endl;
}

Burst::Performance& Burst::Performance::instance()
{
	static Performance performance;
	return performance;
}

float Burst::Performance::Probe::toSeconds() const
{
	if (size == 0)
		return 0.f;

	return std::chrono::duration_cast<std::chrono::duration<float>>(sumTime / size).count();
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
