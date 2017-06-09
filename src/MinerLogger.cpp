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

Burst::MinerLogger::ColorPair Burst::MinerLogger::currentColor = { Color::White, Color::Black };
std::mutex Burst::MinerLogger::consoleMutex;
Burst::TextType Burst::MinerLogger::currentTextType = Burst::TextType::Normal;
bool Burst::MinerLogger::progressFlag_ = false;
float Burst::MinerLogger::lastProgress_ = 0.f;
size_t Burst::MinerLogger::lastPipeCount_ = 0u;

Poco::Logger& Burst::MinerLogger::miner = Poco::Logger::get("miner");
Poco::Logger& Burst::MinerLogger::config = Poco::Logger::get("config");
Poco::Logger& Burst::MinerLogger::server = Poco::Logger::get("server");
Poco::Logger& Burst::MinerLogger::socket = Poco::Logger::get("socket");
Poco::Logger& Burst::MinerLogger::session = Poco::Logger::get("session");
Poco::Logger& Burst::MinerLogger::nonceSubmitter = Poco::Logger::get("nonceSubmitter");
Poco::Logger& Burst::MinerLogger::plotReader = Poco::Logger::get("plotReader");
Poco::Logger& Burst::MinerLogger::plotVerifier = Poco::Logger::get("plotVerifier");
Poco::Logger& Burst::MinerLogger::wallet = Poco::Logger::get("wallet");
Poco::Logger& Burst::MinerLogger::general = Poco::Logger::get("general");

std::unordered_map<uint32_t, bool> Burst::MinerLogger::output_ = {
	{ LastWinner, true },
	{ NonceFound, true },
	{ NonceOnTheWay, true },
	{ NonceSent, true },
	{ NonceConfirmed, true },
	{ PlotDone, false },
	{ DirDone, true },
};

const std::vector<std::string> Burst::MinerLogger::channelNames
{
	"miner", "config", "server", "socket", "session", "nonceSubmitter", "plotReader", "plotVerifier", "wallet", "general"
};

const std::unordered_map<std::string, Burst::MinerLogger::ColoredPriorityConsoleChannel*> Burst::MinerLogger::channels_ = []()
{
	std::unordered_map<std::string, ColoredPriorityConsoleChannel*> channels_;

	for (auto& name : channelNames)
		channels_.insert({name , new ColoredPriorityConsoleChannel{}});

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
		if (priority == "fatal")
			iter->second->setPriority(Poco::Message::PRIO_FATAL);
		else if (priority == "critical")
			iter->second->setPriority(Poco::Message::PRIO_CRITICAL);
		else if (priority == "error")
			iter->second->setPriority(Poco::Message::PRIO_ERROR);
		else if (priority == "warning")
			iter->second->setPriority(Poco::Message::PRIO_WARNING);
		else if (priority == "notice")
			iter->second->setPriority(Poco::Message::PRIO_NOTICE);
		else if (priority == "information")
			iter->second->setPriority(Poco::Message::PRIO_INFORMATION);
		else if (priority == "debug")
			iter->second->setPriority(Poco::Message::PRIO_DEBUG);
		else if (priority == "trace")
			iter->second->setPriority(Poco::Message::PRIO_TRACE);
		else if (priority == "off")
			iter->second->setPriority(static_cast<Poco::Message::Priority>(0));
		else if (priority == "all")
			iter->second->setPriority(Poco::Message::PRIO_TRACE);
		else
			return false;

		return true;
	}

	return false;
}

std::string Burst::MinerLogger::getChannelPriority(const std::string& channel)
{
	auto iter = channels_.find(channel);

	if (iter != channels_.end())
	{
		switch (iter->second->getPriority())
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
		}
	}

	return "";
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

void Burst::MinerLogger::setOutput(int id, bool set)
{
	output_[id] = set;
}

bool Burst::MinerLogger::hasOutput(int id)
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
		clearLine();

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

void Burst::MinerLogger::clearLine()
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

	std::cout << '\r' << std::string(consoleLength, ' ') << '\r' << std::flush;
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
	// create (not open yet) FileChannel
	{
		fileChannel_ = new Poco::FileChannel;

		// rotate every 1 MB
		fileChannel_->setProperty("rotation", "1 M");
		// purge old logs
		fileChannel_->setProperty("purgeAge", "1 days");

		auto filePattern = new Poco::PatternFormatter{"%d.%m.%Y %H:%M:%S (%I, %U, %u, %p): %t"};
		fileFormatter_ = new Poco::FormattingChannel{filePattern, fileChannel_};
	}

	// create all logger channels
	for (auto& name : channelNames)
	{
		auto& logger = Poco::Logger::get(name);
		// set the logger to fetch all messages
		logger.setLevel(Poco::Message::Priority::PRIO_TRACE);

		auto splitter = new Poco::SplitterChannel;
		auto& channel = channels_.at(name);

		splitter->addChannel(channel);
		splitter->addChannel(fileFormatter_);

		Poco::Logger::get(name).setChannel(splitter);
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
