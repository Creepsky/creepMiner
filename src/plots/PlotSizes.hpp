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

#include <map>
#include <Poco/Mutex.h>
#include <Poco/Net/IPAddress.h>
#include <iostream>

namespace Burst
{
	class PlotSizes
	{
	public:
		enum class Type
		{
			Local,
			Remote,
			Combined
		};

	public:
		~PlotSizes() = delete;

		/**
		 * \brief Sets or adds the size of all plot files of a miner.
		 * \param ip The id of the miner.
		 * \param size The size of all plot files.
		 * \param local True, if the plot sizes belongs to the local computer.
		 */
		static void set(const Poco::Net::IPAddress& ip, Poco::UInt64 size, bool local);

		/**
		 * \brief Gets the sum size of all plot files of one miner in the cluster.
		 * \param ip The id of the miner.
		 * \return The amount of plot size in GB (0 if the miner is not in the cluster).
		 */
		static Poco::UInt64 get(const Poco::Net::IPAddress& ip);

		/**
		 * \brief Gets the sum size of all plot files from all miners in the cluster.
		 * \param type Filters the size.
		 * \param maxAge Only count sizes that were updated in the last maxAge rounds.
		 * If 0, all sizes are summed up.
		 * \return The total amount of plot size in GB.
		 */
		static Poco::UInt64 getTotal(Type type, Poco::UInt64 maxAge = 10);
		static Poco::UInt64 getTotalBytes(Type type, Poco::UInt64 maxAge = 10);

		/**
		 * \brief Adds one round to all plots sizes.
		 * This is useful in combination with getTotal().
		 */
		static void nextRound();

		/**
		 * \brief Resets the age of a the plots size of a miner.
		 * \param ip The id of the miner.
		 */
		static void refresh(const Poco::Net::IPAddress& ip);

	private:
		struct HistoricalPlotSize
		{
			Poco::UInt64 size = 0;
			Poco::UInt64 age = 0;
			bool local = false;
		};

		static Poco::Mutex mutex_;
		static std::map<Poco::Net::IPAddress, HistoricalPlotSize> sizes_;
	};
}
