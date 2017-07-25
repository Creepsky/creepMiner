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

#include "ProgressPrinter.hpp"
#include "Message.hpp"
#include "Console.hpp"
#include "MinerLogger.hpp"
#include "MinerUtil.hpp"
#include "mining/MinerConfig.hpp"

void Burst::ProgressPrinter::print(float progress) const
{
	if (MinerConfig::getConfig().getLogOutputType() != LogOutputType::Terminal)
		return;

	if (MinerConfig::getConfig().isFancyProgressBar())
	{
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
	}
	else
		*Console::print()
			<< MinerLogger::getTextTypeColor(TextType::Normal) << getTime() << ": "
			<< "Progress: " << std::fixed << std::setprecision(2) << progress << " %";
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
