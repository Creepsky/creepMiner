#include "Console.hpp"
#include <iostream>
#include "Miner.hpp"

#ifdef _WIN32
#include <wincon.h>
#include <windows.h>
#else
#include <iomanip>
#include <cmath>
#endif

Burst::ConsoleColorPair Burst::Console::currentColor_ = { ConsoleColor::White, ConsoleColor::Black };
std::recursive_mutex Burst::Console::mutex_;

Burst::PrintBlock::PrintBlock(std::ostream& stream, std::recursive_mutex& mutex)
	: stream_(&stream), mutex_(&mutex), finished_(false)
{
	mutex_->lock();
}

Burst::PrintBlock::PrintBlock(PrintBlock&& rhs) noexcept
	: stream_(rhs.stream_), mutex_(rhs.mutex_), finished_(rhs.finished_)
{}

Burst::PrintBlock::~PrintBlock()
{
	finish();
}

const Burst::PrintBlock& Burst::PrintBlock::operator<<(ConsoleColor color) const
{
	std::lock_guard<std::mutex> lock(inner_mutex_);

	if (finished_)
		return *this;

	Console::setColor(color);
	return *this;
}

const Burst::PrintBlock& Burst::PrintBlock::operator<<(ConsoleColorPair color) const
{
	std::lock_guard<std::mutex> lock(inner_mutex_);

	if (finished_)
		return *this;

	Console::setColor(color);
	return *this;
}

const Burst::PrintBlock& Burst::PrintBlock::addTime() const
{
	std::lock_guard<std::mutex> lock(inner_mutex_);

	if (finished_)
		return *this;

	*this << getTime();
	return *this;
}

const Burst::PrintBlock& Burst::PrintBlock::nextLine() const
{
	std::lock_guard<std::mutex> lock(inner_mutex_);

	if (finished_)
		return *this;

	*stream_ << std::endl;
	return *this;
}

const Burst::PrintBlock& Burst::PrintBlock::clearLine(bool wipe) const
{
	std::lock_guard<std::mutex> lock(inner_mutex_);

	if (finished_)
		return *this;

	Console::clearLine(wipe);
	return *this;
}

void Burst::PrintBlock::finish()
{
	std::lock_guard<std::mutex> lock(inner_mutex_);

	if (!finished_)
	{
		finished_ = true;
		mutex_->unlock();
	}
}

void Burst::Console::setColor(ConsoleColor foreground, ConsoleColor background)
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);

#ifdef LOG_TERMINAL
#ifdef _WIN32
	auto hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	WORD color = (static_cast<int>(foreground) & 0x0F) + ((static_cast<int>(background) & 0x0F) << 4);
	SetConsoleTextAttribute(hConsole, color);
#elif defined __linux__
	std::cout << getUnixConsoleCode(foreground);
#endif
	currentColor_ = { foreground, background };
#endif
}

void Burst::Console::setColor(ConsoleColorPair color)
{
	setColor(color.foreground, color.background);
}

void Burst::Console::resetColor()
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);

#ifdef LOG_TERMINAL
#ifdef _WIN32
	setColor(ConsoleColor::White);
#elif defined __linux__
	std::cout << getUnixConsoleCode(ConsoleColor::White) << std::endl;
#endif
#endif
	currentColor_ = { ConsoleColor::White, ConsoleColor::Black };
}

std::string Burst::Console::getUnixConsoleCode(ConsoleColor color)
{
	switch (color)
	{
	case ConsoleColor::Black: return "\033[30m";
	case ConsoleColor::Blue: return "\033[34m";
	case ConsoleColor::Green: return "\033[32m";
	case ConsoleColor::Cyan: return "\033[36m";
	case ConsoleColor::Red: return "\033[31m";
	case ConsoleColor::Magenta: return "\033[35m";
	case ConsoleColor::Brown:
	case ConsoleColor::Yellow: return "\033[33m";
	case ConsoleColor::LightGray: return "\033[30;1m";
	case ConsoleColor::DarkGray: return "\033[30;1m";
	case ConsoleColor::LightBlue: return "\033[34;1m";
	case ConsoleColor::LightGreen: return "\033[32;1m";
	case ConsoleColor::LightCyan: return "\033[36;1m";;
	case ConsoleColor::LightRed: return "\033[31;1m";;
	case ConsoleColor::LightMagenta: return "\033[35;1m";;
	case ConsoleColor::White: return "\033[37;1m";
	}

	return "\033[0m";
}

std::shared_ptr<Burst::PrintBlock> Burst::Console::print()
{
	return std::make_shared<PrintBlock>(std::cout, mutex_);
}

void Burst::Console::clearLine(bool wipe)
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);

#ifdef LOG_TERMINAL
	size_t consoleLength;
#ifdef WIN32
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	consoleLength = csbi.srWindow.Right - csbi.srWindow.Left;
#else
	struct winsize size;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
	consoleLength = size.ws_col;
#endif

	std::cout << '\r';

	if (wipe)
		std::cout << std::string(consoleLength, ' ') << '\r';

	std::cout << std::flush;
#endif
}
