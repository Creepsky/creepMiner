#pragma once

#include <cstdint>
#include <array>
#include <Poco/Platform.h>

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
		std::string nameAndVersionAndOs;
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

		static const Version ProjectVersion;
		static const ProjectData Project;
	};

	template <size_t SZ>
	using BytesArray = std::array<uint8_t, SZ>;
	using ScoopData = BytesArray<Settings::ScoopSize>;
	using GensigData = BytesArray<Settings::HashSize>;
	using HashData = BytesArray<Settings::HashSize>;

	using AccountId = uint64_t;
	using Nonce = uint64_t;
}
