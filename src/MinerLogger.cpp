//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#include "MinerLogger.hpp"
#include <iostream>
#include <mutex>
#include "MinerConfig.hpp"
#include <Poco/NestedDiagnosticContext.h>
#include "MinerUtil.hpp"
#include <sstream>

#ifdef _WIN32
#include <wincon.h>
#include <windows.h>
#else
#include <iomanip>
#include <cmath>
#endif

#include <Poco/Logger.h>
#include <Poco/StringTokenizer.h>
#include <Poco/NumberParser.h>
#include <Poco/FileChannel.h>
#include <Poco/PatternFormatter.h>
#include <Poco/FormattingChannel.h>
#include <Poco/SplitterChannel.h>
#include <Poco/File.h>
#include "Output.hpp"
#include <fstream>
#include <Poco/FileStream.h>
#include "MinerData.hpp"

Burst::MinerLogger::ColorPair Burst::MinerLogger::currentColor = { Color::White, Color::Black };
std::mutex Burst::MinerLogger::consoleMutex;
Burst::TextType Burst::MinerLogger::currentTextType = Burst::TextType::Normal;
bool Burst::MinerLogger::progressFlag_ = false;
float Burst::MinerLogger::lastProgress_ = 0.f;
size_t Burst::MinerLogger::lastPipeCount_ = 0u;

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

const std::unordered_map<std::string, Burst::MinerLogger::ColoredPriorityConsoleChannel*> Burst::MinerLogger::channels_ = []()
{
	std::unordered_map<std::string, ColoredPriorityConsoleChannel*> channels_;

	for (auto& channel : channelDefinitions)
		channels_.insert({channel.name , new ColoredPriorityConsoleChannel{channel.default_priority}});

	return channels_;
}();

const std::unordered_map<std::string, Burst::MinerLogger::MinerDataChannel*> Burst::MinerLogger::websocketChannels_ = []()
{
	std::unordered_map<std::string, MinerDataChannel*> channels_;

	for (auto& channel : channelDefinitions)
		channels_.insert({channel.name , new MinerDataChannel{nullptr}});

	return channels_;
}();

Poco::Channel* Burst::MinerLogger::fileChannel_ = new Poco::FileChannel{"startup.log"};
Poco::FormattingChannel* Burst::MinerLogger::fileFormatter_ = nullptr;
std::string Burst::MinerLogger::logFileName_ = getFilenameWithtimestamp("creepMiner", "log");

std::map<Burst::MinerLogger::TextType, Burst::MinerLogger::ColorPair> Burst::MinerLogger::typeColors =
	{
			{ TextType::Normal, { Color::White, Color::Black } },
			{ TextType::Error, { Color::LightRed, Color::Black } },
			{ TextType::Information, { Color::LightCyan, Color::Black } },
			{ TextType::Success, { Color::LightGreen, Color::Black } },
			{ TextType::Warning, { Color::Brown, Color::Black } },
			{ TextType::Important, { Color::Black, Color::White } },
			{ TextType::System, { Color::Yellow, Color::Black } },
			{ TextType::Unimportant, { Color::DarkGray, Color::Black } },
			{ TextType::Ok, { Color::Green, Color::Black } },
			{ TextType::Debug, { Color::LightMagenta, Color::Black } },
			{ TextType::Progress, { Color::DarkGray, Color::Black } },
	};

void Burst::MinerLogger::print(const std::string& text)
{
	auto typeBefore = currentTextType;

	setColor(TextType::Normal);
	printTime();

	setColor(typeBefore);
	std::cout << text;

	if (!progressFlag_)
		std::cout << std::endl;
}

void Burst::MinerLogger::writeAndFunc(TextType type, std::function<void()> func)
{		
	std::lock_guard<std::mutex> lock(consoleMutex);
	setColor(type);

	auto wasProgress = progressFlag_;

	if (wasProgress)
		clearLine();

	progressFlag_ = type == TextType::Progress;

	func();

	if (wasProgress && lastProgress_ < 100.f)
		printProgress(lastProgress_, lastPipeCount_);
}

Poco::FastMutex Burst::MinerLogger::ColoredPriorityConsoleChannel::mutex_;

Burst::MinerLogger::ColoredPriorityConsoleChannel::ColoredPriorityConsoleChannel()
	: ColoredPriorityConsoleChannel{Poco::Message::Priority::PRIO_INFORMATION}
{}

Burst::MinerLogger::ColoredPriorityConsoleChannel::ColoredPriorityConsoleChannel(Poco::Message::Priority priority)
	: priority_ {priority}
{}

void Burst::MinerLogger::ColoredPriorityConsoleChannel::log(const Poco::Message& msg)
{
	Poco::ScopedLock<Poco::FastMutex> lock {mutex_};

	if (priority_ >= msg.getPriority())
	{
		Poco::StringTokenizer tokenizer{msg.getText(), "\n"};
		auto type = TextType::Normal;
		
		if (msg.has("type"))
			type = static_cast<TextType>(Poco::NumberParser::parse(msg.get("type")));

		auto condition = true;

		if (msg.has("condition"))
			condition = Poco::NumberParser::parseBool(msg.get("condition"));

		if (condition)
			for (auto& token : tokenizer)
				write(token, type);
	}
}

void Burst::MinerLogger::ColoredPriorityConsoleChannel::setPriority(Poco::Message::Priority priority)
{
	priority_ = priority;
}

Poco::Message::Priority Burst::MinerLogger::ColoredPriorityConsoleChannel::getPriority() const
{
	return priority_;
}

Burst::MinerLogger::MinerDataChannel::MinerDataChannel()
	: minerData_{nullptr}
{}

Burst::MinerLogger::MinerDataChannel::MinerDataChannel(MinerData* minerData)
	: minerData_{minerData}
{}

void Burst::MinerLogger::MinerDataChannel::log(const Poco::Message& msg)
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

void Burst::MinerLogger::MinerDataChannel::setMinerData(MinerData* minerData)
{
	minerData_ = minerData;
}

Burst::MinerData* Burst::MinerLogger::MinerDataChannel::getMinerData() const
{
	return minerData_;
}

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
	writeAndFunc(type, [text]()
	{
		print(text);
	});
}

void Burst::MinerLogger::write(const std::vector<std::string>& lines, TextType type)
{
	writeAndFunc(type, [lines]()
	{
		for (const auto& line : lines)
			print(line);
	});
}

void Burst::MinerLogger::writeProgress(float progress, size_t pipes)
{
	std::lock_guard<std::mutex> lock(consoleMutex);

	if (progressFlag_)
		clearLine(false);

	printProgress(progress, pipes);
}

void Burst::MinerLogger::nextLine()
{
	write("");
}

void Burst::MinerLogger::setColor(Color foreground, Color background)
{
#ifdef LOG_TERMINAL
#ifdef _WIN32
	auto hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	WORD color = ((static_cast<int32_t>(foreground) & 0x0F) + ((static_cast<int32_t>(background) & 0x0F) << 4));
	SetConsoleTextAttribute(hConsole, color);
	currentColor = { foreground, background };
#endif
#endif
}

void Burst::MinerLogger::setColor(TextType type)
{
#ifdef LOG_TERMINAL
#ifdef _WIN32
	setColor(getTextTypeColor(type));
#elif __linux__
	switch (type)
	{
	case TextType::Normal: std::cout << "\033[0;37m"; break;
	case TextType::Information: std::cout << "\033[1;36m"; break;
	case TextType::Error: std::cout << "\033[1;31m"; break;
	case TextType::Success: std::cout << "\033[1;32m"; break;
	case TextType::Warning: std::cout << "\033[0m"; break;
	case TextType::Important: std::cout << "\033[1;47;37m"; break;
	case TextType::System: std::cout << "\033[0;33m"; break;
	case TextType::Unimportant: std::cout << "\033[2;37m"; break;
	case TextType::Ok: std::cout << "\033[0;32m"; break;
	case TextType::Debug: std::cout << "\033[0;35m"; break;
	case TextType::Progress: std::cout << "\033[2;37m"; break;
	default: std::cout << "\033[0;37m"; break;
	};
#endif
#endif
	currentTextType = type;
}

void Burst::MinerLogger::clearLine(bool wipe)
{
#ifdef LOG_TERMINAL
	static auto consoleLength = 0u;

	if (consoleLength == 0)
	{
#ifdef WIN32
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
		consoleLength = csbi.srWindow.Right - csbi.srWindow.Left;
#else
		struct winsize size;
		ioctl(STDOUT_FILENO,TIOCGWINSZ, &size);
		consoleLength = size.ws_col;
#endif
	}

	std::cout << '\r';

	if (wipe)
		 std::cout << std::string(consoleLength, ' ') << '\r';

	std::cout << std::flush;
#endif
}

void Burst::MinerLogger::printTime()
{
	std::cout << getTime() << ": ";
}

void Burst::MinerLogger::printProgress(float progress, size_t pipes)
{
#ifdef LOG_TERMINAL
	auto done = static_cast<size_t>(pipes * (progress / 100));
	auto notDone = pipes - done;
	lastProgress_ = progress;
	lastPipeCount_ = pipes;
	progressFlag_ = true;

	setColor(TextType::Normal);
	printTime();

	setColor(TextType::Unimportant);
	std::cout << '[';

	setColor(TextType::Success);
	std::cout << std::string(done, '+');

	setColor(TextType::Normal);
	std::cout << std::string(notDone, '-');

	setColor(TextType::Unimportant);
	std::cout << ']' << ' ' << static_cast<size_t>(progress) << '%' << std::flush;
#endif
}

void Burst::MinerLogger::setColor(ColorPair color)
{
	setColor(color.foreground, color.background);
}

void Burst::MinerLogger::setTextTypeColor(TextType type, ColorPair color)
{
	std::lock_guard<std::mutex> lock(consoleMutex);
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

		auto filePattern = new Poco::PatternFormatter{ "%d.%m.%Y %H:%M:%S (%I, %U, %u, %p): %t" };
		filePattern->setProperty("times", "local");
		fileFormatter_ = new Poco::FormattingChannel{ filePattern, fileChannel_ };
	}

	refreshChannels();
}

void Burst::MinerLogger::refreshChannels()
{
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

	setLogDir("");
}

Burst::MinerLogger::ColorPair Burst::MinerLogger::getTextTypeColor(TextType type)
{
	auto iter = typeColors.find(type);

	if (iter == typeColors.end())
		return getTextTypeColor(TextType::Normal);

	return (*iter).second;
}

Burst::MinerLogger& Burst::MinerLogger::getInstance()
{
	static MinerLogger instance;
	return instance;
}

void Burst::MinerLogger::writeStackframe(const std::string& additionalText)
{
	std::stringstream ss;
	ss << "Stackframe" << std::endl << std::endl;
	Poco::NDC::current().dump(ss);
	
	std::vector<std::string> lines = {
		additionalText,
		ss.str()
	};
	
	write(lines, TextType::Error);
}

void Burst::MinerLogger::writeStackframe(std::vector<std::string>& additionalLines) 
{
	std::stringstream ss;
	ss << "Stackframe" << std::endl << std::endl;
	Poco::NDC::current().dump(ss);

	additionalLines.emplace_back(ss.str());

	write(additionalLines, TextType::Error);
}
