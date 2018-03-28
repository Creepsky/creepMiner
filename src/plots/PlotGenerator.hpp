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
#include "webserver/MinerServer.hpp"

namespace Burst
{
	class Miner;

	class PlotGenerator
	{
	public:
		static Poco::UInt64 generateAndCheck(Poco::UInt64 account, Poco::UInt64 nonce, const Miner& miner);
		static void checkPlotfileIntegrity(std::string plotPath, Miner& miner, MinerServer& server);
	};

	/*template <typename TShabal>
	class PlotGeneratorQueue
	{
	public:
		struct QueueSlot
		{
			Poco::UInt64 account;
			Poco::UInt64 nonce;
			Poco::UInt64 deadline;
		};

	public:
		PlotGeneratorQueue()
		{
			queue_.reserve(TShabal::HashSize);
		}

		static Poco::UInt64 add(Poco::UInt64 account, Poco::UInt64 nonce)
		{
			std::unique_lock<std::mutex> lock(mutex_);
			
			queue_[index_].account = account;
			queue_[index_].nonce = nonce;
			queue_[index_].deadline = 0;

			const auto thisIndex = index_++;

			if (index_ == TShabal::HashSize)
			{
				index_ = 0;
				condition_.notify_all();
				auto lastDeadline = queue_.back().deadline;
				std::lock_guard<std::mutex> lock(mutex_);
				return lastDeadline;
			}

			condition_.wait(lock);

			return queue_[thisIndex].deadline;
		}

		static void setMiner(const Miner& miner)
		{
			std::lock_guard<std::mutex> lock(mutex_);
			miner_ = &miner;
		}

	private:
		static std::mutex mutex_;
		static std::condition_variable condition_;
		static const Miner* miner_;
		static int index_;
		static std::vector<QueueSlot> queue_;
	};

	template <typename TShabal>
	struct PlotGenerator_Algorithm
	{
		Poco::UInt64 generate(Poco::UInt64 account, Poco::UInt64 nonce)
		{
			char final[32];
			char gendata[16 + Settings::PlotSize];

			auto xv = reinterpret_cast<char*>(&account);

			for (auto j = 0u; j <= 7; ++j)
				gendata[Settings::PlotSize + j] = xv[7 - j];

			xv = reinterpret_cast<char*>(&nonce);

			for (auto j = 0u; j <= 7; ++j)
				gendata[Settings::PlotSize + 8 + j] = xv[7 - j];

			for (auto i = Settings::PlotSize; i > 0; i -= Settings::HashSize)
			{
				TShabal x;

				auto len = Settings::PlotSize + 16 - i;

				if (len > Settings::ScoopPerPlot)
					len = Settings::ScoopPerPlot;

				x.update(&gendata[i], len);
				x.close(&gendata[i - Settings::HashSize]);
			}

			TShabal x;
			x.update(&gendata[0], 16 + Settings::PlotSize);
			x.close(&final[0]);

			// XOR with final
			for (auto i = 0; i < Settings::PlotSize; i++)
				gendata[i] ^= final[i % 32];

			std::array<uint8_t, 32> target;
			Poco::UInt64 result;

			const auto generationSignature = miner.getGensig();
			const auto scoop = miner.getScoopNum();
			const auto basetarget = miner.getBaseTarget();

			TShabal y;
			y.update(generationSignature.data(), Settings::HashSize);
			y.update(&gendata[scoop * Settings::ScoopSize], Settings::ScoopSize);
			y.close(target.data());

			memcpy(&result, target.data(), sizeof(Poco::UInt64));

			return result / basetarget;
		}
	};*/
}
