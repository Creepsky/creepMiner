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

#pragma once

#include <cstdio>

namespace Burst
{
	struct ProgressPrinter
	{
		/**
		* \brief The front delimiter.
		*/
		char delimiterFront = '[';

		/**
		* \brief A char that represents the filled part of the progress bar.
		*/
		char doneChar = '+';

		/**
		* \brief A char that represents the not filled  part of the progress bar.
		*/
		char notDoneChar = '-';

		/**
		* \brief The end delimiter.
		*/
		char delimiterEnd = ']';

		/**
		 * \brief The size of the filled and not filled part of the progressbar combined.
		 */
		size_t totalSize = 48;

		/**
		 * \brief Prints the progress bar.
		 */
		void print(float progress) const;

		/**
		* \brief Calculates the proportion of the filled and not filled part of a progress bar.
		* \param progress The progress, where 0 <= progress <= 100.
		* \param totalSize The whole size of the progress bar.
		* It will be split it by done and not done (doneSize + notDoneSize = totalSize).
		* \param doneSize The proportion of the filled part.
		* \param notDoneSize The proportion of the not filled part.
		*/
		static void calculateProgressProportions(float progress, size_t totalSize, size_t& doneSize, size_t& notDoneSize);
	};
}
