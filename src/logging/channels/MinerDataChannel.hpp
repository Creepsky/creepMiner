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

#include <Poco/Channel.h>

namespace Burst
{
	class MinerData;

	/**
	 * \brief A logging channel that logs messages into the \class MinerData
	 */
	class MinerDataChannel : public Poco::Channel
	{
	public:
		/**
		 * \brief Constructor.
		 * The miner data is not defined, therefore no messages will be logged.
		 */
		MinerDataChannel();

		/**
		 * \brief Constructor.
		 * \param minerData The miner data, in which the channel will log messages.
		 */
		explicit MinerDataChannel(MinerData* minerData);

		/**
		 * \brief Destructor.
		 */
		~MinerDataChannel() override = default;

		/**
		 * \brief Logs a messages.
		 * When no miner data is defined, the message is discarded.
		 * \param msg The message.
		 */
		void log(const Poco::Message& msg) override;

		/**
		 * \brief Sets the destination miner data, in which the channel will log the messages.
		 * \param minerData The miner data.
		 */
		void setMinerData(MinerData* minerData);

		/**
		 * \brief Returns the miner data, in which the channel logs messages.
		 * \return The miner data.
		 */
		MinerData* getMinerData() const;

	private:
		MinerData* minerData_;
	};
}
