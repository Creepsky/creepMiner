//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#include "MinerLogger.h"
#include <iostream>
#include <iomanip>
#include <mutex>
#include "MinerConfig.h"
#include <iomanip>

#ifdef _WIN32
#include <win/dirent.h>
#include <wincon.h>
#endif


Burst::MinerLogger::ColorPair Burst::MinerLogger::currentColor = { Color::White, Color::Black };
std::mutex Burst::MinerLogger::consoleMutex;
Burst::TextType Burst::MinerLogger::currentTextType = Burst::TextType::Normal;

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
	};

void Burst::MinerLogger::print(const std::string& text)
{
	auto now = std::chrono::system_clock::now();
	auto now_c = std::chrono::system_clock::to_time_t(now);

	auto typeBefore = currentTextType;

	setColor(TextType::Normal);
	
	struct tm newtime;
	localtime_s(&newtime, &now_c);
	
	std::cout << std::put_time(&newtime, "%X") << ": ";

	setColor(typeBefore);
	std::cout << text << std::endl;
}

void Burst::MinerLogger::write(const std::string& text, TextType type)
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
	print(text);
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
	default: std::cout << "\033[0;37m"; break;
	};
#endif
	currentTextType = type;
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
