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

#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <Poco/Platform.h>
#include <Poco/Types.h>
#include <Poco/Timer.h>

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
		bool operator==(const Version& rhs) const;
		bool operator!=(const Version& rhs) const;

		uint32_t major, minor, build, revision;
		std::string literal, literalVerbose;

	private:
		void updateLiterals();
	};

	struct ProjectData
	{
		ProjectData(std::string&& name, Version version);
		void refreshNameAndVersion();
		void refreshAndCheckOnlineVersion(Poco::Timer& timer);
		std::string getOnlineVersion() const;
		const Version& getVersion() const;
		std::string name;
		std::string nameAndVersion;
		std::string nameAndVersionVerbose;
	private:
		Version version_;
		Version onlineVersion_;
		mutable Poco::Mutex mutex_;
	};

	namespace Settings
	{
		// 32 bytes, half of scoop
		constexpr size_t hashSize = 32;
		// there are 4096 scoops inside a plot (nonce)
		constexpr size_t scoopPerPlot = 4096;
		// every scoop consists of 2 hashes
		constexpr size_t hashPerScoop = 2;
		// 64 bytes, whole scoop, two hashes
		constexpr size_t scoopSize = hashSize * hashPerScoop;
		// 262144 bytes, a nonce, 4096 scoops
		constexpr size_t plotSize = scoopSize * scoopPerPlot;
		// 96 bytes, a scoop and the gensig
		constexpr size_t plotScoopSize = scoopSize + hashSize;

#if defined POCO_OS_FAMILY_BSD
		static constexpr auto osFamily = "BSD";
#elif defined POCO_OS_FAMILY_UNIX
		static constexpr auto osFamily = "Unix";
#elif defined POCO_OS_FAMILY_WINDOWS
		static constexpr auto osFamily = "Windows";
#else
		static constexpr auto osFamily = "";
#endif

#if POCO_ARCH == POCO_ARCH_IA64 || POCO_ARCH == POCO_ARCH_AMD64 || POCO_ARCH == POCO_ARCH_AARCH64
		static constexpr auto arch = "x64";
#else
		static constexpr auto arch = "x32";
#endif

		extern std::string cpuInstructionSet;
		extern ProjectData project;

		extern const bool sse4, avx, avx2, cuda, openCl;

		void setCpuInstructionSet(std::string cpuInstructionSet);
	};

	template <size_t Sz>
	using BytesArray = std::array<uint8_t, Sz>;
	using ScoopData = BytesArray<Settings::scoopSize>;
	using GensigData = BytesArray<Settings::hashSize>;
	using HashData = BytesArray<Settings::hashSize>;

	using AccountId = Poco::UInt64;
	using Nonce = Poco::UInt64;

	enum class LogOutputType
	{
		Terminal,
		Service
	};
}
