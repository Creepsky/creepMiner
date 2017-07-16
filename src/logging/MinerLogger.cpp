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

#include "MinerLogger.hpp"
#include <iostream>
#include <mutex>
#include "mining/MinerConfig.hpp"
#include <Poco/NestedDiagnosticContext.h>
#include "MinerUtil.hpp"
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <iomanip>
#include <cmath>
#endif

#include <Poco/Logger.h>
#include <Poco/FileChannel.h>
#include <Poco/PatternFormatter.h>
#include <Poco/FormattingChannel.h>
#include <Poco/SplitterChannel.h>
#include <Poco/File.h>
#include "Output.hpp"
#include <fstream>
#include <Poco/FileStream.h>
#include "mining/MinerData.hpp"
#include "Console.hpp"
#include "channels/MinerDataChannel.hpp"
#include "ProgressPrinter.hpp"
#include <Poco/StringTokenizer.h>

std::recursive_mutex Burst::MinerLogger::mutex_;
Burst::TextType Burst::MinerLogger::currentTextType_ = Burst::TextType::Normal;
bool Burst::MinerLogger::progressFlag_ = false;
float Burst::MinerLogger::lastProgress_ = 0.f;
size_t Burst::MinerLogger::lastPipeCount_ = 0u;
Burst::ProgressPrinter Burst::MinerLogger::progressPrinter_;

Poco::Logger* Burst::MinerLogger::miner;
Poco::Logger* Burst::MinerLogger::config;
Poco::Logger* Burst::MinerLogger::server;
Poco::Logger* Burst::MinerLogger::socket;
Poco::Logger* Burst::MinerLogger::session;
Poco::Logger* Burst::MinerLogger::nonceSubmitter;
Poco::Logger* Burst::MinerLogger::plotReader;
Poco::Logger* Burst::MinerLogger::plotVerifier;
Poco::Logger* Burst::MinerLogger::wallet;
Poco::Logger* Burst::MinerLogger::general;

Burst::Output_Flags Burst::MinerLogger::output_ = []()
{
	auto output_flags = Output_Helper::create_flags(true);
	output_flags[PlotDone] = false;
	return output_flags;
}();

const std::vector<Burst::MinerLogger::ChannelDefinition> Burst::MinerLogger::channelDefinitions =
{
	{ "miner", Poco::Message::Priority::PRIO_INFORMATION },
	{ "config", Poco::Message::Priority::PRIO_INFORMATION },
	{ "server", Poco::Message::Priority::PRIO_FATAL },
	{ "socket", static_cast<Poco::Message::Priority>(0) },
	{ "session", Poco::Message::Priority::PRIO_ERROR },
	{ "nonceSubmitter", Poco::Message::Priority::PRIO_INFORMATION },
	{ "plotReader", Poco::Message::Priority::PRIO_INFORMATION },
	{ "plotVerifier", Poco::Message::Priority::PRIO_INFORMATION },
	{ "wallet", Poco::Message::Priority::PRIO_FATAL },
	{ "general", Poco::Message::Priority::PRIO_INFORMATION },
};

const std::unordered_map<std::string, Burst::ColoredPriorityConsoleChannel*> Burst::MinerLogger::channels_ = []()
{
	std::unordered_map<std::string, ColoredPriorityConsoleChannel*> channels_;

	for (auto& channel : channelDefinitions)
		channels_.insert({channel.name , new ColoredPriorityConsoleChannel{channel.default_priority}});

	return channels_;
}();

const std::unordered_map<std::string, Burst::MinerDataChannel*> Burst::MinerLogger::websocketChannels_ = []()
{
	std::unordered_map<std::string, MinerDataChannel*> channels_;

	for (auto& channel : channelDefinitions)
		channels_.insert({channel.name , new MinerDataChannel{nullptr}});

	return channels_;
}();

Poco::Channel* Burst::MinerLogger::fileChannel_ = new Poco::FileChannel{"startup.log"};
Poco::FormattingChannel* Burst::MinerLogger::fileFormatter_ = nullptr;
std::string Burst::MinerLogger::logFileName_ = getFilenameWithtimestamp("creepMiner", "log");

std::map<Burst::TextType, Burst::ConsoleColorPair> Burst::MinerLogger::typeColors =
	{
			{ TextType::Normal, { ConsoleColor::White, ConsoleColor::Black } },
			{ TextType::Error, { ConsoleColor::LightRed, ConsoleColor::Black } },
			{ TextType::Information, { ConsoleColor::LightCyan, ConsoleColor::Black } },
			{ TextType::Success, { ConsoleColor::LightGreen, ConsoleColor::Black } },
			{ TextType::Warning, { ConsoleColor::Brown, ConsoleColor::Black } },
			{ TextType::Important, { ConsoleColor::Black, ConsoleColor::White } },
			{ TextType::System, { ConsoleColor::Yellow, ConsoleColor::Black } },
#ifdef _WIN32
			{ TextType::Unimportant, { ConsoleColor::DarkGray, ConsoleColor::Black } },
#else
			{ TextType::Unimportant, { ConsoleColor::LightGray, ConsoleColor::Black } },
#endif
			{ TextType::Ok, { ConsoleColor::Green, ConsoleColor::Black } },
			{ TextType::Debug, { ConsoleColor::LightMagenta, ConsoleColor::Black } },
			{ TextType::Progress, { ConsoleColor::DarkGray, ConsoleColor::Black } },
	};

Burst::MinerLogger::ChannelDefinition::ChannelDefinition(std::string name, Poco::Message::Priority default_priority)
	: name{std::move(name)},
	  default_priority{default_priority} {}

const Burst::Output_Flags& Burst::MinerLogger::getOutput()
{
	return output_;
}

bool Burst::MinerLogger::setChannelPriority(const std::string& channel, Poco::Message::Priority priority)
{
	auto iter = channels_.find(channel);

	if (iter != channels_.end())
	{
		iter->second->setPriority(priority);
		return true;
	}

	return false;
}

bool Burst::MinerLogger::setChannelPriority(const std::string& channel, const std::string& priority)
{
	auto iter = channels_.find(channel);

	if (iter != channels_.end())
	{
		iter->second->setPriority(getStringToPriority(priority));
		return true;
	}

	return false;
}

std::string Burst::MinerLogger::getChannelPriority(const std::string& channel)
{
	auto iter = channels_.find(channel);

	if (iter != channels_.end())
		return getPriorityToString(iter->second->getPriority());

	return "";
}

Poco::Message::Priority Burst::MinerLogger::getStringToPriority(const std::string& priority)
{
	if (priority == "fatal")
		return Poco::Message::PRIO_FATAL;
	if (priority == "critical")
		return Poco::Message::PRIO_CRITICAL;
	if (priority == "error")
		return Poco::Message::PRIO_ERROR;
	if (priority == "warning")
		return Poco::Message::PRIO_WARNING;
	if (priority == "notice")
		return Poco::Message::PRIO_NOTICE;
	if (priority == "information")
		return Poco::Message::PRIO_INFORMATION;
	if (priority == "debug")
		return Poco::Message::PRIO_DEBUG;
	if (priority == "trace")
		return Poco::Message::PRIO_TRACE;
	if (priority == "off")
		return static_cast<Poco::Message::Priority>(0);
	if (priority == "all")
		return Poco::Message::PRIO_TRACE;
	
	return static_cast<Poco::Message::Priority>(0);
}

std::string Burst::MinerLogger::getPriorityToString(Poco::Message::Priority priority)
{
	if (static_cast<int>(priority) == 0)
		return "off";

	if (static_cast<int>(priority) > static_cast<int>(Poco::Message::Priority::PRIO_TRACE))
		return "all";

	switch (priority)
	{
	case Poco::Message::PRIO_FATAL:
		return "fatal";
	case Poco::Message::PRIO_CRITICAL:
		return "critical";
	case Poco::Message::PRIO_ERROR:
		return "error";
	case Poco::Message::PRIO_WARNING:
		return "warning";
	case Poco::Message::PRIO_NOTICE:
		return "notice";
	case Poco::Message::PRIO_INFORMATION:
		return "information";
	case Poco::Message::PRIO_DEBUG:
		return "debug";
	case Poco::Message::PRIO_TRACE:
		return "trace";
	default:
		return "";
	}
}

std::map<std::string, std::string> Burst::MinerLogger::getChannelPriorities()
{
	std::map<std::string, std::string> channel_priorities;

	for (auto& channel : channels_)
		channel_priorities.emplace(channel.first, getChannelPriority(channel.first));

	return channel_priorities;
}

std::string Burst::MinerLogger::setLogDir(const std::string& dir)
{
	try
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);

		Poco::Path fullPath;
		fullPath.parseDirectory(dir);

		if (!fullPath.toString().empty())
		{
			Poco::File file{ fullPath };

			if (!file.exists())
				file.createDirectories();
		}
		
		fullPath.append(logFileName_);
		auto logPath = fullPath.toString();

		fileChannel_->close();

		auto oldLogfilePath = fileChannel_->getProperty("path");

		if (!oldLogfilePath.empty())
		{
			std::ifstream oldLogfile{ oldLogfilePath, std::ios::in | std::ios::binary | std::ios::ate };

			if (oldLogfile)
			{
				auto fileSize = oldLogfile.tellg();
				oldLogfile.seekg(0, std::ios::beg);

				std::vector<char> bytes(fileSize);
				oldLogfile.read(&bytes[0], fileSize);

                std::string oldContent{&bytes[0], static_cast<size_t>(fileSize)};

				oldLogfile.close();

				Poco::File oldLogfileObj{ oldLogfilePath };
				oldLogfileObj.remove();

				std::ofstream newLogfile{ logPath, std::ios::out | std::ios::binary };
				newLogfile << oldContent;
				newLogfile.close();
			}
		}

		fileChannel_->setProperty("path", logPath);
		fileChannel_->open();

		return logPath;
	}
	catch (...)
	{
		throw;
	}
}

void Burst::MinerLogger::setChannelMinerData(MinerData* minerData)
{
	for (auto& channel : websocketChannels_)
		channel.second->setMinerData(minerData);
}

Poco::FormattingChannel* Burst::MinerLogger::getFileFormattingChannel()
{
	return fileFormatter_;
}

void Burst::MinerLogger::setOutput(Burst::Output id, bool set)
{
	output_[id] = set;
}

bool Burst::MinerLogger::hasOutput(Burst::Output id)
{
	auto iter = output_.find(id);

	if (iter != output_.end())
		return iter->second;

	return false;
}

void Burst::MinerLogger::write(const std::string& text, TextType type)
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);

	auto block = Console::print();

	auto wasProgress = progressFlag_;

	if (wasProgress)
		block->clearLine();

	progressFlag_ = type == TextType::Progress;
	
	Poco::StringTokenizer tokenizer(text, "\n");
	
	for (auto i = 0u; i < tokenizer.count(); ++i)
	{
		*block << getTextTypeColor(TextType::Normal) << getTime() << ": "
			<< getTextTypeColor(type) << tokenizer[i];

		if (i != tokenizer.count() - 1)
			block->nextLine();

		block->resetColor();
	}

	if (!progressFlag_)
		block->nextLine();

	if (wasProgress && lastProgress_ < 100.f)
	{
		progressPrinter_.print(lastProgress_);
		progressFlag_ = true;
	}
}

void Burst::MinerLogger::writeProgress(float progress)
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);

	if (static_cast<size_t>(progress) == static_cast<size_t>(lastProgress_))
		return;

	if (progressFlag_)
		Console::print()->clearLine(false);

	lastProgress_ = progress;
	progressFlag_ = true;

	progressPrinter_.print(progress);
}

void Burst::MinerLogger::setTextTypeColor(TextType type, ConsoleColorPair color)
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	typeColors[type] = color;
}

void Burst::MinerLogger::setup()
{
	miner = &Poco::Logger::get("miner");
	config = &Poco::Logger::get("config");
	server = &Poco::Logger::get("server");
	socket = &Poco::Logger::get("socket");
	session = &Poco::Logger::get("session");
	nonceSubmitter = &Poco::Logger::get("nonceSubmitter");
	plotReader = &Poco::Logger::get("plotReader");
	plotVerifier = &Poco::Logger::get("plotVerifier");
	wallet = &Poco::Logger::get("wallet");
	general = &Poco::Logger::get("general");

	// create (not open yet) FileChannel
	{
		fileChannel_ = new Poco::FileChannel;

		// rotate every 1 MB
		fileChannel_->setProperty("rotation", "1 M");
		// purge old logs
		fileChannel_->setProperty("purgeAge", "1 days");
		// use local times
		fileChannel_->setProperty("times", "local");
		// archive old logfiles
		// fileChannel_->setProperty("compress", "true");

		auto filePattern = new Poco::PatternFormatter{ "%d.%m.%Y %H:%M:%S (%I, %U, %u, %p): %t" };
		filePattern->setProperty("times", "local");
		fileFormatter_ = new Poco::FormattingChannel{ filePattern, fileChannel_ };
	}

	refreshChannels();
	setLogDir("");
}

void Burst::MinerLogger::refreshChannels()
{
	// close the filechannel if not used
	if (!MinerConfig::getConfig().isLogfileUsed() &&
		fileChannel_ != nullptr)
		fileChannel_->close();

	// create all logger channels
	for (auto& channel : channelDefinitions)
	{
		auto& logger = Poco::Logger::get(channel.name);
		// set the logger to fetch all messages
		logger.setLevel(Poco::Message::Priority::PRIO_TRACE);

		auto splitter = new Poco::SplitterChannel;
		auto& consoleChannel = channels_.at(channel.name);
		auto& websocketChannel = websocketChannels_.at(channel.name);

		splitter->addChannel(consoleChannel);

		if (MinerConfig::getConfig().getStartServer())
			splitter->addChannel(websocketChannel);

		if (MinerConfig::getConfig().isLogfileUsed())
			splitter->addChannel(fileFormatter_);

		Poco::Logger::get(channel.name).setChannel(splitter);
	}
}

Burst::ConsoleColorPair Burst::MinerLogger::getTextTypeColor(TextType type)
{
	auto iter = typeColors.find(type);

	if (iter == typeColors.end())
		return getTextTypeColor(TextType::Normal);

	return (*iter).second;
}

void Burst::MinerLogger::writeStackframe(const std::string& additionalText)
{
	std::stringstream sstream;

	sstream << additionalText << std::endl
		<< "Stackframe" << std::endl << std::endl;

	Poco::NDC::current().dump(sstream);
		
	write(sstream.str(), TextType::Error);
}
