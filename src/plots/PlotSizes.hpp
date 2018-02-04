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
#include <string>
#include <unordered_map>
#include <Poco/Mutex.h>

namespace Burst
{
	class PlotSizes
	{
	public:
		~PlotSizes() = delete;

		/**
		 * \brief Sets or adds the size of all plot files of a miner.
		 * \param plotsHash The id of the miner.
		 * \param size The size of all plot files.
		 */
		static void set(const std::string& plotsHash, Poco::UInt64 size);

		/**
		 * \brief Gets the sum size of all plot files of one miner in the cluster.
		 * \param plotsHash The id of the miner.
		 * \return The amount of plot size in GB (0 if the miner is not in the cluster).
		 */
		static Poco::UInt64 get(const std::string& plotsHash);

		/**
		 * \brief Gets the sum size of all plot files from all miners in the cluster.
		 * \param maxAge Only count sizes that were updated in the last maxAge rounds.
		 * If 0, all sizes are summed up.
		 * \return The total amount of plot size in GB.
		 */
		static Poco::UInt64 getTotal(Poco::UInt64 maxAge = 10);

		/**
		 * \brief Adds one round to all plots sizes.
		 * This is useful in combination with getTotal().
		 */
		static void nextRound();

		/**
		 * \brief Resets the age of a the plots size of a miner.
		 * \param plotsHash The id of the miner.
		 */
		static void refresh(const std::string& plotsHash);

	private:
		struct HistoricalPlotSize
		{
			Poco::UInt64 size = 0;
			Poco::UInt64 age = 0;
		};

		static Poco::Mutex mutex_;
		static std::unordered_map<std::string, HistoricalPlotSize> sizes_;
	};
}
