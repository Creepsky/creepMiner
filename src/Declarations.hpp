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
		Version(uint32_t major, uint32_t minor, uint32_t build, uint32_t revision);
		Version(std::string version);

		bool operator>(const Version& rhs) const;

		uint32_t major, minor, build, revision;
		std::string literal, literalVerbose;

	private:
		void updateLiterals();
	};

	struct ProjectData
	{
		ProjectData(std::string&& name, Version version);
		void refreshNameAndVersion();

		std::string name;
		Version version;

		std::string nameAndVersion;
		std::string nameAndVersionVerbose;
	};

	namespace Settings
	{
		// 32 bytes, half of scoop
		constexpr size_t HashSize = 32;
		// there are 4096 scoops inside a plot (nonce)
		constexpr size_t ScoopPerPlot = 4096;
		// every scoop consists of 2 hashes
		constexpr size_t HashPerScoop = 2;
		// 64 bytes, whole scoop, two hashes
		constexpr size_t ScoopSize = HashSize * HashPerScoop;
		// 262144 bytes, a nonce, 4096 scoops
		constexpr size_t PlotSize = ScoopSize * ScoopPerPlot;
		// 96 bytes, a scoop and the gensig
		constexpr size_t PlotScoopSize = ScoopSize + HashSize;

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

		extern std::string Cpu_Instruction_Set;
		extern const Version ProjectVersion;
		extern ProjectData Project;

		extern const bool Sse4, Avx, Avx2, Cuda, OpenCl;

		void setCpuInstructionSet(std::string cpuInstructionSet);
	};

	template <size_t SZ>
	using BytesArray = std::array<uint8_t, SZ>;
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
