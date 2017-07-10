#include "ProgressPrinter.hpp"
#include <iostream>
#include "Message.hpp"
#include "Console.hpp"
#include "MinerLogger.hpp"
#include "MinerUtil.hpp"

void Burst::ProgressPrinter::print(float progress) const
{
#ifdef LOG_TERMINAL
	// calculate the progress bar proportions
	size_t doneSize, notDoneSize;
	calculateProgressProportions(progress, totalSize, doneSize, notDoneSize);

	*Console::print()
		<< MinerLogger::getTextTypeColor(TextType::Normal) << getTime() << ": "
		<< MinerLogger::getTextTypeColor(TextType::Unimportant) << delimiterFront
		<< MinerLogger::getTextTypeColor(TextType::Success) << std::string(doneSize, doneChar)
		<< MinerLogger::getTextTypeColor(TextType::Normal) << std::string(totalSize - doneSize, notDoneChar)
		<< MinerLogger::getTextTypeColor(TextType::Unimportant) << delimiterEnd
		<< ' ' << static_cast<size_t>(progress) << '%';
#endif
}

void Burst::ProgressPrinter::calculateProgressProportions(float progress, size_t totalSize, size_t& doneSize, size_t& notDoneSize)
{
	// precatch division by zero
	if (progress == 0.f)
	{
		doneSize = 0;
		notDoneSize = 0;
		return;
	}

	doneSize = static_cast<size_t>(totalSize * (progress / 100));
	notDoneSize = totalSize - doneSize;
}
