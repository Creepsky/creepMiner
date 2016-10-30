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

#ifdef _WIN32
#include <win/dirent.h>
#include <wincon.h>
#endif

Burst::MinerLogger::ColorPair Burst::MinerLogger::currentColor = { Color::White, Color::Black };
std::mutex Burst::MinerLogger::consoleMutex;

std::map<Burst::MinerLogger::TextType, Burst::MinerLogger::ColorPair> Burst::MinerLogger::typeColors =
{
	{ TextType::Normal, { Color::White, Color::Black }},
	{ TextType::Error, { Color::LightRed, Color::Black }},
	{ TextType::Information, { Color::LightCyan, Color::Black }},
	{ TextType::Success, { Color::LightGreen, Color::Black }},
	{ TextType::Warning, { Color::Yellow, Color::Black }},
	{ TextType::Important, { Color::Black, Color::White }},
	{ TextType::System, { Color::Yellow, Color::Black }},
	{ TextType::Unimportant, { Color::DarkGray, Color::Black }},
	{ TextType::Ok, { Color::Green, Color::Black }},
};

void Burst::MinerLogger::print(const std::string& text)
{
	auto now = std::chrono::system_clock::now();
	auto now_c = std::chrono::system_clock::to_time_t(now);

	auto colorBefore = currentColor;

	setColor(getTextTypeColor(TextType::Normal));
	std::cout << std::put_time(std::localtime(&now_c), "%X") << ": ";

	setColor(colorBefore);
	std::cout << text << std::endl;
}

void Burst::MinerLogger::write(const std::string& text, TextType type)
{
    std::lock_guard<std::mutex> lock(consoleMutex);
	setColor(getTextTypeColor(type));
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
