#include "Output.hpp"
#include <algorithm>

const std::map<Burst::Output, std::string> Burst::Output_Helper::Output_Names = []()
{
	return std::map<Burst::Output, std::string> {
		{LastWinner, "lastWinner"},
		{NonceFound, "nonceFound"},
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
		{NonceSent, default_flag},
		{NonceConfirmed, default_flag},
		{PlotDone, default_flag},
		{DirDone, default_flag}
	};
}
