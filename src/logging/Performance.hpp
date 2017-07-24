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

#include <map>
#include <mutex>
#include <chrono>
#include <ostream>
#include <string>

namespace Burst
{
	class Performance
	{
	public:
		/**
		 * \brief Restarts the start point of measurement.
		 * \param id The id of the measurement.
		 */
		void reset(const std::string &id);

		/**
		 * \brief Clears all measurements.
		 */
		void clear();
		
		/**
		 * \brief Adds a time measurement.
		 * Has no effect, when the id was not initialized with \see reset.
		 * \param id The id of the measurement.
		 */
		void takeProbe(const std::string &id);
	
		/**
		 * \brief Prints the whole measurements into a output stream.
		 * \param stream The output stream.
		 */
		void print(std::ostream& stream) const;

		/**
		 * \brief Prints the whole measurements into a output stream.
		 * \param stream The output stream.
		 * \param performance The performance instance, that is printed.
		 * \return The output stream.
		 */
		friend std::ostream& operator<<(std::ostream& stream, const Performance& performance);
	
		/**
		 * \brief Returns the global performance instance.
		 * \return The global singleton instance
		 */
		static Performance& instance();

	private:
		struct Probe
		{
			std::chrono::high_resolution_clock::time_point currentTime;
			std::chrono::high_resolution_clock::duration sumTime;
			std::chrono::high_resolution_clock::duration lowestTime;
			std::chrono::high_resolution_clock::duration highestTime;
			size_t size;

			/**
			 * \brief Returns the average duration of the probes.
			 * \return The average duration as a float number.
			 */
			float avgToSeconds() const;

			/**
			* \brief Returns the lowest duration of the probes.
			* \return The lowest duration as a float number.
			*/
			float lowestToSeconds() const;

			/**
			* \brief Returns the highest duration of the probes.
			* \return The highest duration as a float number.
			*/
			float highestToSeconds() const;

			/**
			 * \brief Returns the sum duration of all the probes.
			 * \return The sum duration as a float number.
			 */
			float sumToSeconds() const;
		};
		
		std::map<std::string, Probe> probes_;
		mutable std::mutex mutex_;
	};
}

#ifdef RUN_BENCHMARK
/**
 * \brief Starts a new probe with a specific name.
 * \param name The name of the probe.
 */
#define START_PROBE(name) Burst::Performance::instance().reset(name);

/**
 * \brief Starts a new probe with a specific name and also a probe for a domain.
 * \param name The name of the probe.
 * \param domain The domain of the probe.
 */
#define START_PROBE_DOMAIN(name, domain) START_PROBE(name) START_PROBE(std::string(name) + "." + domain)

/**
 * \brief Takes a probe with a specific name.
 * First, you need to call START_PROBE
 * \param name The name of the probe.
 */
#define TAKE_PROBE(name) Burst::Performance::instance().takeProbe(name);

/**
 * \brief Takes a probe with a specific name and also a probe for a specific domain.
 * First, you need to call START_PROBE_DOMAIN
 * \param name The name of the probe.
 * \param domain The domain of the probe.
 */
#define TAKE_PROBE_DOMAIN(name, domain) TAKE_PROBE(name) TAKE_PROBE(std::string(name) + "." + domain);

/**
 * \brief Removes all probes.
 */
#define CLEAR_PROBES() Burst::Performance::instance().clear();
#else
#define START_PROBE(name) void();
#define START_PROBE_DOMAIN(name, domain) void();
#define TAKE_PROBE(name) void();
#define TAKE_PROBE_DOMAIN(name, domain) void();
#define CLEAR_PROBES() void();
#endif
