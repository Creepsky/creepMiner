#pragma once

#include <Poco/Types.h>
#include <map>

namespace Burst
{
	enum Output : Poco::UInt32
	{
		LastWinner,
		NonceFound,
		NonceOnTheWay,
		NonceSent,
		NonceConfirmed,
		PlotDone,
		DirDone
	};

	template <typename T>
	using Output_Map = std::map<Output, T>;
	using Output_Flags = Output_Map<bool>;

	struct Output_Helper
	{
		static const std::map<Output, std::string> Output_Names;
		static std::string output_to_string(Output output);
		static Output string_to_output(const std::string& output);
		static Output_Flags create_flags(bool default_flag = true);
	};	
}
