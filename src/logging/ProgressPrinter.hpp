#pragma once

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
