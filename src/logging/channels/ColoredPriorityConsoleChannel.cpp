#include "ColoredPriorityConsoleChannel.hpp"
#include <Poco/NumberParser.h>
#include <Poco/StringTokenizer.h>
#include "logging/Message.hpp"
#include "logging/MinerLogger.hpp"

Poco::FastMutex Burst::ColoredPriorityConsoleChannel::mutex_;

Burst::ColoredPriorityConsoleChannel::ColoredPriorityConsoleChannel()
	: ColoredPriorityConsoleChannel{ Poco::Message::Priority::PRIO_INFORMATION }
{}

Burst::ColoredPriorityConsoleChannel::ColoredPriorityConsoleChannel(Poco::Message::Priority priority)
	: priority_{ priority }
{}

void Burst::ColoredPriorityConsoleChannel::log(const Poco::Message& msg)
{
	Poco::ScopedLock<Poco::FastMutex> lock{ mutex_ };

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
