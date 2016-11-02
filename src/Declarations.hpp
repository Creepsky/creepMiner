#pragma once

#include <cstdint>
#include <array>

namespace Burst
{
	class MinerShabal;
	class Miner;
	class MinerLogger;
	class MinerConfig;
	class MinerProtocol;
	class PlotReader;

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
	};

	template <size_t SZ>
	using BytesArray = std::array<uint8_t, SZ>;
	using ScoopData = BytesArray<Settings::ScoopSize>;
	using GensigData = BytesArray<Settings::HashSize>;
	using HashData = BytesArray<Settings::HashSize>;

	using AccountId = uint64_t;
	using Nonce = uint64_t;

	class Version
	{
	public:
		static const uint32_t Major = 1u, Minor = 2u, Build = 5u;
	};
}
