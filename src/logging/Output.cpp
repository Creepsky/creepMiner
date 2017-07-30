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

#include "Output.hpp"
#include <algorithm>

const std::map<Burst::Output, std::string> Burst::Output_Helper::Output_Names = []()
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

using Output_Map_Pair_t = decltype(Burst::Output_Helper::Output_Names)::value_type;

std::string Burst::Output_Helper::output_to_string(Output output)
{
	auto iter = std::find_if(Output_Names.begin(), Output_Names.end(), [&](const Output_Map_Pair_t& output_pair)
	{
		return output_pair.first == output;
	});

	return iter == Output_Names.end() ? "" : iter->second;
}

Burst::Output Burst::Output_Helper::string_to_output(const std::string& output)
{
	auto iter = std::find_if(Output_Names.begin(), Output_Names.end(), [&](const Output_Map_Pair_t& output_pair)
	{
		return output_pair.second == output;
	});

	return iter == Output_Names.end() ? Output() : iter->first;
}

Burst::Output_Flags Burst::Output_Helper::create_flags(bool default_flag)
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
