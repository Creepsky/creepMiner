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
