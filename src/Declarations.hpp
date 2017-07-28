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

#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <Poco/Platform.h>
#include <Poco/Types.h>

namespace Burst
{
	class MinerShabal;
	class Miner;
	class MinerLogger;
	class MinerConfig;
	class MinerProtocol;
	class PlotReader;

	struct Version
	{
		Version(uint32_t major, uint32_t minor, uint32_t build);
		Version(std::string version);

		bool operator>(const Version& rhs) const;

		uint32_t major, minor, build;
		std::string literal;
	};

	struct ProjectData
	{
		ProjectData(std::string&& name, Version version);

		std::string name;
		Version version;

		std::string nameAndVersion;
		std::string nameAndVersionVerbose;
	};

	class Settings
	{
	public:
		Settings() = delete;

		static const size_t HashSize = 32;
		static const size_t ScoopPerPlot = 4096;
		static const size_t HashPerScoop = 2;
		static const size_t ScoopSize = HashPerScoop * HashSize; // 64 Bytes
		static const size_t PlotSize = ScoopPerPlot * ScoopSize; // 256KB = 262144 Bytes
		static const size_t PlotScoopSize = ScoopSize + HashSize; // 64 + 32 bytes

#if defined POCO_OS_FAMILY_BSD
		static constexpr auto OsFamily = "BSD";
#elif defined POCO_OS_FAMILY_UNIX
		static constexpr auto OsFamily = "Unix";
#elif defined POCO_OS_FAMILY_WINDOWS
		static constexpr auto OsFamily = "Windows";
#else
		static constexpr auto OsFamily = "";
#endif

#if POCO_ARCH == POCO_ARCH_IA64 || POCO_ARCH == POCO_ARCH_AMD64 || POCO_ARCH == POCO_ARCH_AARCH64
		static constexpr auto Arch = "x64";
#else
		static constexpr auto Arch = "x32";
#endif

		static const std::string Cpu_Instruction_Set;
		static const Version ProjectVersion;
		static const ProjectData Project;
	};

	template <size_t SZ>
	using BytesArray = std::array<char, SZ>;
	using ScoopData = BytesArray<Settings::ScoopSize>;
	using GensigData = BytesArray<Settings::HashSize>;
	using HashData = BytesArray<Settings::HashSize>;

	using AccountId = Poco::UInt64;
	using Nonce = Poco::UInt64;

	enum class LogOutputType
	{
		Terminal,
		Service
	};
}
