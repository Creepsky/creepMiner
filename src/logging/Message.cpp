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

#include "Message.hpp"
#include <Poco/Format.h>
#include <sstream>
#include <memory>
#include "MinerLogger.hpp"
#include "mining/MinerConfig.hpp"
#include <memory>
#include "MinerUtil.hpp"

Poco::NotificationQueue Burst::Message::messageQueue_;

Burst::MessageNotification::MessageNotification(Poco::Logger& logger, Poco::Message message)
	: message(std::move(message)), logger(&logger)
{}

void Burst::Message::log(Poco::Message::Priority priority, TextType type, Poco::Logger& logger, const std::string& text, const char* file, int line, bool condition)
{
	createAndLog(priority, type, logger, text, file, line, condition);
}

void Burst::Message::log(Poco::Message::Priority priority, TextType type, Poco::Logger& logger, Poco::Exception& exception, const char* file, int line)
{
	log(priority, type, logger, Poco::format("Exception occured: %s\n\terror-code: %d\n\tclass: %s",
	                                         exception.displayText(), exception.code(), std::string(exception.className())), file, line);
}

void Burst::Message::log(Poco::Message::Priority priority, TextType type, Poco::Logger& logger, const Poco::NestedDiagnosticContext& stackframe, const char* file, int line)
{
	Message::stackframe(priority, type, logger, stackframe, file, line);
}

void Burst::Message::log(Poco::Message::Priority priority, TextType type, Poco::Logger& logger, const std::string& text, const void* memory, size_t size, const char* file, int line)
{
	logger.dump(text, memory, size);
}

void Burst::Message::logIntoFile(Poco::Message::Priority priority, TextType type, Poco::Logger& logger, const std::string& text, const char* file, int line)
{
	// if the logfile is not used, we cant log into it
	if (!MinerConfig::getConfig().isLogfileUsed())
		return;

	auto fileChannel = MinerLogger::getFileFormattingChannel();

	// check if the filechannel is open and active
	if (fileChannel != nullptr )
	{
		// create the message..
		auto message = create(priority, type, logger, text, file, line);
		// ..and log it
		fileChannel->log(message);
	}
}

void Burst::Message::wakeUpAllDispatcher()
{
	messageQueue_.wakeUpAll();
}

Burst::Message::Dispatcher::Dispatcher(Poco::NotificationQueue& queue)
	: Poco::Task("Message-Dispatcher"), queue_(&queue)
{}

void Burst::Message::Dispatcher::runTask()
{
	while (!isCancelled())
	{
		// wait for an incoming message...
		Poco::Notification::Ptr notification(queue_->waitDequeueNotification());
		MessageNotification::Ptr messageNotification;

		if (notification)
			// cast to the right notification type
			messageNotification = notification.cast<MessageNotification>();
		else
			// wake up call (a higher instance is forcing us to terminate)
			return;

		// finally log it
		messageNotification->logger->log(messageNotification->message);
	}
}

std::unique_ptr<Burst::Message::Dispatcher> Burst::Message::Dispatcher::create()
{
	return std::make_unique<Dispatcher>(messageQueue_);
}

Poco::Message Burst::Message::create(Poco::Message::Priority priority, TextType type, Poco::Logger& logger, const std::string& text, const char* file, int line, bool condition)
{
	Poco::Message message;
	message.setText(text);
	message.setPriority(priority);
	message.set("type", std::to_string(static_cast<int>(type)));
	message.set("condition", std::to_string(condition));
	message.setSource(logger.name());
	message.setSourceFile(file);
	message.setSourceLine(line);
	return message;
}

void Burst::Message::log(Poco::Logger& logger, const Poco::Message& message)
{
	logger.log(message);

	// it is possible to make this whole process async by replacing
	// logger.log(message) with this line of code:
	//messageQueue_.enqueueNotification(new MessageNotification(logger, message));
	// then also an instance of the Message::Dispatcher needs to be created in the main
}

void Burst::Message::createAndLog(Poco::Message::Priority priority, TextType type, Poco::Logger& logger, const std::string& text, const char* file, int line, bool condition)
{
	log(logger, create(priority, type, logger, text, file, line, condition));
}

void Burst::Message::stackframe(Poco::Message::Priority priority, TextType type, Poco::Logger& logger, const Poco::NestedDiagnosticContext& stackframe, const char* file, int line)
{
	std::stringstream ss;
	ss << "Stackframe" << std::endl;
	stackframe.dump(ss);
	const auto stackframeString = ss.str();

	log(priority, type, logger, stackframeString.substr(0, stackframeString.size() - 1), file, line);
}

Burst::Message::Message() = default;
