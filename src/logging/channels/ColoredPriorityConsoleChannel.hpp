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

#include <Poco/Message.h>
#include <Poco/Mutex.h>
#include <Poco/Channel.h>

namespace Burst
{
	/**
	 * \brief A logging channel that logs messages into the \class Console
	 */
	class ColoredPriorityConsoleChannel : public Poco::Channel
	{
	public:
		/**
		 * \brief Constructor.
		 * The default priority INFORMATION is set.
		 */
		ColoredPriorityConsoleChannel();

		/**
		 * \brief Constructor.
		 * \param priority The priority of the channel.
		 */
		explicit ColoredPriorityConsoleChannel(Poco::Message::Priority priority);

		/**
		 * \brief Destructor.
		 */
		~ColoredPriorityConsoleChannel() override = default;

		/**
		 * \brief Logs a messages.
		 * \param msg The message.
		 */
		void log(const Poco::Message& msg) override;

		/**
		 * \brief Sets the priority of the channel.
		 * \param priority The new priority.
		 */
		void setPriority(Poco::Message::Priority priority);

		/**
		 * \brief Returns the priority of the channel.
		 * \return The priority.
		 */
		Poco::Message::Priority getPriority() const;

	private:
		static Poco::FastMutex mutex_;
		Poco::Message::Priority priority_;
	};
}
