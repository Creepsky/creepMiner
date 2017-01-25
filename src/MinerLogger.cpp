//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#include "MinerLogger.hpp"
#include <iostream>
#include <iomanip>
#include <mutex>
#include "MinerConfig.hpp"
#include <iomanip>
#include <cmath>
#include <Poco/NestedDiagnosticContext.h>
#include "MinerUtil.hpp"

#ifdef _WIN32
#include <wincon.h>
#include <windows.h>
#endif


Burst::MinerLogger::ColorPair Burst::MinerLogger::currentColor = { Color::White, Color::Black };
std::mutex Burst::MinerLogger::consoleMutex;
Burst::TextType Burst::MinerLogger::currentTextType = Burst::TextType::Normal;
bool Burst::MinerLogger::progressFlag_ = false;
float Burst::MinerLogger::lastProgress_ = 0.f;
size_t Burst::MinerLogger::lastPipeCount_ = 0u;

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
	switch (type)
	{
	case TextType::Debug:
		if (!MinerConfig::getConfig().output.debug)
			return;
	default: break;
	}
		
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
#ifdef _WIN32
	auto hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	WORD color = ((static_cast<int32_t>(foreground) & 0x0F) + ((static_cast<int32_t>(background) & 0x0F) << 4));
	SetConsoleTextAttribute(hConsole, color);
	currentColor = { foreground, background };
#endif
}

void Burst::MinerLogger::setColor(TextType type)
{
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
	currentTextType = type;
}

void Burst::MinerLogger::clearLine()
{
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
}

void Burst::MinerLogger::printTime()
{
	std::cout << getTime() << ": ";
}

void Burst::MinerLogger::printProgress(float progress, size_t pipes)
{
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
	ss << std::endl << "Stackframe" << std::endl << std::endl;
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
