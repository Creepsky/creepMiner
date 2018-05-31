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

#include <Poco/Types.h>
#include <mutex>
#include <vector>
#include <condition_variable>
#include "Declarations.hpp"
#include "shabal/MinerShabal.hpp"
#include <future>
#include <cstring>

namespace Burst
{
	class Miner;
	class MinerServer;

	class PlotGenerator
	{
	public:
		static Poco::UInt64 generateAndCheck(Poco::UInt64 account, Poco::UInt64 nonce, const Miner& miner);
		static double checkPlotfileIntegrity(const std::string& plotPath, Miner& miner, MinerServer& server);

		static std::array<std::vector<char>, Shabal256Sse2::HashSize> generateSse2(Poco::UInt64 account, Poco::UInt64 startNonce);
		static std::array<std::vector<char>, Shabal256Avx::HashSize> generateAvx(Poco::UInt64 account, Poco::UInt64 startNonce);
		static std::array<std::vector<char>, Shabal256Sse4::HashSize> generateSse4(Poco::UInt64 account, Poco::UInt64 startNonce);
		static std::array<std::vector<char>, Shabal256Avx2::HashSize> generateAvx2(Poco::UInt64 account, Poco::UInt64 startNonce);

		static Poco::UInt64 calculateDeadlineSse2(std::vector<char>& gendata,
			GensigData& generationSignature, Poco::UInt64 scoop, Poco::UInt64 baseTarget);

		static std::array<Poco::UInt64, Shabal256Avx::HashSize> calculateDeadlineAvx(
			std::array<std::vector<char>, Shabal256Avx::HashSize>& gendatas,
			GensigData& generationSignature, Poco::UInt64 scoop, Poco::UInt64 baseTarget);

		static std::array<Poco::UInt64, Shabal256Sse4::HashSize> calculateDeadlineSse4(
			std::array<std::vector<char>, Shabal256Sse4::HashSize>& gendatas,
			GensigData& generationSignature, Poco::UInt64 scoop, Poco::UInt64 baseTarget);

		static std::array<Poco::UInt64, Shabal256Avx2::HashSize> calculateDeadlineAvx2(
			std::array<std::vector<char>, Shabal256Avx2::HashSize>& gendatas,
			GensigData& generationSignature, Poco::UInt64 scoop, Poco::UInt64 baseTarget);

	private:
		template <typename TShabal, typename TOperations>
		static std::array<std::vector<char>, TShabal::HashSize> generate(const Poco::UInt64 account, const Poco::UInt64 startNonce)
		{
			std::array<std::array<char, 32>, TShabal::HashSize> finals{};
			std::array<std::vector<char>, TShabal::HashSize> gendatas{};

			for (auto& gendata : gendatas)
				gendata.resize(16 + Settings::plotSize);

			auto xv = reinterpret_cast<const char*>(&account);

			for (auto& gendata : gendatas)
				for (auto j = 0u; j <= 7; ++j)
					gendata[Settings::plotSize + j] = xv[7 - j];

			auto nonce = startNonce;

			for (auto& gendata : gendatas)
			{
				xv = reinterpret_cast<char*>(&nonce);
				
				for (auto j = 0u; j <= 7; ++j)
					gendata[Settings::plotSize + 8 + j] = xv[7 - j];

				++nonce;
			}

			std::array<unsigned char*, TShabal::HashSize> gendataUpdatePtr{};
			std::array<unsigned char*, TShabal::HashSize> gendataClosePtr{};

			for (auto i = Settings::plotSize; i > 0; i -= Settings::hashSize)
			{
				TShabal x;

				auto len = Settings::plotSize + 16 - i;

				if (len > Settings::scoopPerPlot)
					len = Settings::scoopPerPlot;

				for (size_t j = 0; j < TShabal::HashSize; ++j)
				{
					gendataUpdatePtr[j] = reinterpret_cast<unsigned char*>(gendatas[j].data() + i);
					gendataClosePtr[j] = reinterpret_cast<unsigned char*>(gendatas[j].data() + i - Settings::hashSize);
				}

				TOperations::update(x, gendataUpdatePtr, len);
				TOperations::close(x, gendataClosePtr);
			}

			TShabal x;

			for (size_t i = 0; i < TShabal::HashSize; ++i)
			{
				gendataUpdatePtr[i] = reinterpret_cast<unsigned char*>(gendatas[i].data());
				gendataClosePtr[i] = reinterpret_cast<unsigned char*>(&finals[i][0]);
			}

			TOperations::update(x, gendataUpdatePtr, 16 + Settings::plotSize);
			TOperations::close(x, gendataClosePtr);

			// XOR with final
			for (size_t i = 0; i < TShabal::HashSize; ++i)
				for (size_t j = 0; j < Settings::plotSize; j++)
					gendatas[i][j] ^= finals[i][j % Settings::hashSize];

			return std::move(gendatas);
		}

		template <typename TShabal, typename TOperations, typename TContainer>
		static std::array<Poco::UInt64, TShabal::HashSize> calculateDeadline(TContainer& container,
			GensigData& generationSignature, const Poco::UInt64 scoop, const Poco::UInt64 baseTarget)
		{
			std::array<std::array<uint8_t, 32>, TShabal::HashSize> targets{};
			std::array<Poco::UInt64, TShabal::HashSize> results{};

			std::array<char*, TShabal::HashSize> gensigPtr{};
			std::array<char*, TShabal::HashSize> updatePtr{};
			std::array<char*, TShabal::HashSize> closePtr{};

			for (size_t i = 0; i < TShabal::HashSize; ++i)
			{
				gensigPtr[i] = reinterpret_cast<char*>(generationSignature.data());
				updatePtr[i] = &container[i].at(scoop * Settings::scoopSize);
				closePtr[i] = reinterpret_cast<char*>(targets[i].data());
			}

			TShabal x;
			TOperations::update(x, gensigPtr, Settings::hashSize);
			TOperations::update(x, updatePtr, Settings::scoopSize);
			TOperations::close(x, closePtr);
			
			std::array<Poco::UInt64, TShabal::HashSize> deadlines{};

			for (size_t i = 0; i < TShabal::HashSize; ++i)
			{
				memcpy(&deadlines[i], targets[i].data(), sizeof(Poco::UInt64));
				deadlines[i] /= baseTarget;
			}

			return deadlines;
		}

		static void convertToPoC2(char* gendata);

		template <typename TContainer>
		static void convertToPoC2(TContainer& container)
		{
			std::vector<std::thread> workers;

			for (auto& gendata : container)
			{
				workers.emplace_back([&]()
				{
					convertToPoC2(gendata);
				});
			}

			for (auto& worker : workers)
				worker.join();
		}
	};

	template <typename TShabal>
	struct PlotGeneratorOperations1
	{
		template <typename TContainer>
		static void update(TShabal& shabal, const TContainer& container, const Poco::UInt64 length)
		{
			shabal.update(container[0], length);
		}

		template <typename TContainer>
		static void close(TShabal& shabal, const TContainer& container)
		{
			shabal.close(container[0]);
		}
	};

	template <typename TShabal>
	struct PlotGeneratorOperations4
	{
		template <typename TContainer>
		static void update(TShabal& shabal, const TContainer& container, const Poco::UInt64 length)
		{
			shabal.update(container[0], container[1], container[2], container[3], length);
		}

		template <typename TContainer>
		static void close(TShabal& shabal, const TContainer& container)
		{
			shabal.close(container[0], container[1], container[2], container[3]);
		}
	};

	template <typename TShabal>
	struct PlotGeneratorOperations8
	{
		template <typename TContainer>
		static void update(TShabal& shabal, const TContainer& container, const Poco::UInt64 length)
		{
			shabal.update(container[0], container[1], container[2], container[3],
				container[4], container[5], container[6], container[7], length);
		}

		template <typename TContainer>
		static void close(TShabal& shabal, const TContainer& container)
		{
			shabal.close(container[0], container[1], container[2], container[3],
				container[4], container[5], container[6], container[7]);
		}
	};
}
