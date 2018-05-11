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

#include "Output.hpp"
#include <algorithm>

const std::map<Burst::Output, std::string> Burst::OutputHelper::outputNames = []()
{
	return std::map<Burst::Output, std::string> {
		{LastWinner, "lastWinner"},
		{NonceFound, "nonceFound"},
		{NonceFoundTooHigh, "nonceFoundTooHigh"},
		{NonceSent, "nonceSent"},
		{NonceConfirmed, "nonceConfirmed"},
		{PlotDone, "plotDone"},
		{DirDone, "dirDone"}
	};
}();

using Output_Map_Pair_t = decltype(Burst::OutputHelper::outputNames)::value_type;

std::string Burst::OutputHelper::outputToString(Output output)
{
	auto iter = std::find_if(outputNames.begin(), outputNames.end(), [&](const Output_Map_Pair_t& output_pair)
	{
		return output_pair.first == output;
	});

	return iter == outputNames.end() ? "" : iter->second;
}

Burst::Output Burst::OutputHelper::stringToOutput(const std::string& output)
{
	auto iter = std::find_if(outputNames.begin(), outputNames.end(), [&](const Output_Map_Pair_t& output_pair)
	{
		return output_pair.second == output;
	});

	return iter == outputNames.end() ? Output() : iter->first;
}

Burst::OutputFlags Burst::OutputHelper::createFlags(bool default_flag)
{
	return {
		{LastWinner, default_flag},
		{NonceFound, default_flag},
		{NonceFoundTooHigh, default_flag},
		{NonceSent, default_flag},
		{NonceConfirmed, default_flag},
		{PlotDone, default_flag},
		{DirDone, default_flag}
	};
}
