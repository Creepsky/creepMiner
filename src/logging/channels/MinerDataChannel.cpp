#include "MinerDataChannel.hpp"
#include <Poco/Message.h>
#include "MinerData.hpp"

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
