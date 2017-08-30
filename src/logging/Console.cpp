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


#include "Console.hpp"
#include <iostream>
#include "mining/Miner.hpp"
#include "MinerUtil.hpp"
#include "mining/MinerConfig.hpp"

#ifdef _WIN32
#include <wincon.h>
#include <windows.h>
#else
#include <iomanip>
#include <cmath>
#endif

Burst::PrintBlock::PrintBlock(std::ostream& stream)
	: stream_(&stream)
{
}

const Burst::PrintBlock& Burst::PrintBlock::operator<<(ConsoleColor color) const
{
	Console::setColor(color);
	return *this;
}

const Burst::PrintBlock& Burst::PrintBlock::operator<<(ConsoleColorPair color) const
{
	Console::setColor(color);
	return *this;
}

const Burst::PrintBlock& Burst::PrintBlock::addTime() const
{
	*this << getTime();
	return *this;
}

const Burst::PrintBlock& Burst::PrintBlock::nextLine() const
{
	*stream_ << std::endl;
	return *this;
}

const Burst::PrintBlock& Burst::PrintBlock::resetColor() const
{
	Console::resetColor();
	return *this;
}

const Burst::PrintBlock& Burst::PrintBlock::clearLine(bool wipe) const
{
	Console::clearLine(wipe);
	return *this;
}

const Burst::PrintBlock& Burst::PrintBlock::flush() const
{
	*stream_ << std::flush;
	return *this;
}

Burst::PrintBlock::~PrintBlock()
{
	flush();
	resetColor();
}

void Burst::Console::setColor(ConsoleColor foreground, ConsoleColor background)
{
	if (MinerConfig::getConfig().getLogOutputType() != LogOutputType::Terminal ||
		!MinerConfig::getConfig().isUsingLogColors())
		return;

#ifdef _WIN32
	auto hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	WORD color = (static_cast<int>(foreground) & 0x0F) + ((static_cast<int>(background) & 0x0F) << 4);
	SetConsoleTextAttribute(hConsole, color);
#elif defined __linux__
	std::cout << getUnixConsoleCode(foreground);
#endif
}

void Burst::Console::setColor(ConsoleColorPair color)
{
	setColor(color.foreground, color.background);
}

void Burst::Console::resetColor()
{
	if (MinerConfig::getConfig().getLogOutputType() != LogOutputType::Terminal ||
		!MinerConfig::getConfig().isUsingLogColors())
		return;

#ifdef _WIN32
	setColor(ConsoleColor::White);
#elif defined __linux__
	std::cout << "\033[0m";
#endif
}

std::string Burst::Console::getUnixConsoleCode(ConsoleColor color)
{
	switch (color)
	{
	case ConsoleColor::Black: return "\033[30m";
	case ConsoleColor::Blue: return "\033[34m";
	case ConsoleColor::Green: return "\033[0;32m";
	case ConsoleColor::Cyan: return "\033[36m";
	case ConsoleColor::Red: return "\033[31m";
	case ConsoleColor::Magenta: return "\033[35m";
	case ConsoleColor::Brown:
	case ConsoleColor::Yellow: return "\033[33;1m";
	case ConsoleColor::LightGray: return "\033[2;37m";
	case ConsoleColor::DarkGray: return "\033[30;1m";
	case ConsoleColor::LightBlue: return "\033[34;1m";
	case ConsoleColor::LightGreen: return "\033[32;1m";
	case ConsoleColor::LightCyan: return "\033[36;1m";;
	case ConsoleColor::LightRed: return "\033[31;1m";;
	case ConsoleColor::LightMagenta: return "\033[35;1m";;
	case ConsoleColor::White: return "\033[37;1m";
	default: return "\033[0m";
	}
}

Burst::PrintBlock Burst::Console::print()
{
	return PrintBlock{ std::cout };
}

void Burst::Console::clearLine(bool wipe)
{
	if (MinerConfig::getConfig().getLogOutputType() != LogOutputType::Terminal)
		return;

	size_t consoleLength = 0;

#ifdef WIN32
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	consoleLength = csbi.srWindow.Right - csbi.srWindow.Left;
#else
	winsize size;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) > -1)
		consoleLength = size.ws_col;
#endif

	if (consoleLength > 0)
	{
		std::cout << '\r';

		if (wipe)
			std::cout << std::string(consoleLength, ' ') << '\r';

		std::cout << std::flush;
	}
}

void Burst::Console::nextLine()
{
	std::cout << std::endl;
}
