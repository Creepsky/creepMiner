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

#include "ColoredPriorityConsoleChannel.hpp"
#include <Poco/NumberParser.h>
#include <Poco/StringTokenizer.h>
#include "logging/Message.hpp"
#include "logging/MinerLogger.hpp"

Poco::Mutex Burst::ColoredPriorityConsoleChannel::mutex_;

Burst::ColoredPriorityConsoleChannel::ColoredPriorityConsoleChannel()
	: ColoredPriorityConsoleChannel{ Poco::Message::Priority::PRIO_INFORMATION }
{}

Burst::ColoredPriorityConsoleChannel::ColoredPriorityConsoleChannel(Poco::Message::Priority priority)
	: priority_{ priority }
{}

void Burst::ColoredPriorityConsoleChannel::log(const Poco::Message& msg)
{
	Poco::ScopedLock<Poco::Mutex> lock{ mutex_ };

	if (priority_ >= msg.getPriority())
	{
		Poco::StringTokenizer tokenizer{ msg.getText(), "\n" };
		auto type = TextType::Normal;

		if (msg.has("type"))
			type = static_cast<TextType>(Poco::NumberParser::parse(msg.get("type")));

		auto condition = true;

		if (msg.has("condition"))
			condition = Poco::NumberParser::parseBool(msg.get("condition"));

		if (condition)
			MinerLogger::write(msg.getText(), type);
	}
}

void Burst::ColoredPriorityConsoleChannel::setPriority(Poco::Message::Priority priority)
{
	priority_ = priority;
}

Poco::Message::Priority Burst::ColoredPriorityConsoleChannel::getPriority() const
{
	return priority_;
}
