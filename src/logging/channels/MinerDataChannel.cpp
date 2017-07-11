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

#include "MinerDataChannel.hpp"
#include <Poco/Message.h>
#include "mining/MinerData.hpp"

Burst::MinerDataChannel::MinerDataChannel()
	: minerData_{ nullptr }
{}

Burst::MinerDataChannel::MinerDataChannel(MinerData* minerData)
	: minerData_{ minerData }
{}

void Burst::MinerDataChannel::log(const Poco::Message& msg)
{
	if (minerData_ == nullptr)
		return;

	// we dont send informations and notices, because for them are build custom JSON objects
	// so that the webserver can react appropriate
	if (msg.getPriority() == Poco::Message::PRIO_INFORMATION ||
		msg.getPriority() == Poco::Message::PRIO_NOTICE)
		return;

	minerData_->addMessage(msg);
}

void Burst::MinerDataChannel::setMinerData(MinerData* minerData)
{
	minerData_ = minerData;
}

Burst::MinerData* Burst::MinerDataChannel::getMinerData() const
{
	return minerData_;
}
