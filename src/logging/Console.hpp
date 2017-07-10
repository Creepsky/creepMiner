#pragma once
#include "channels/ColoredPriorityConsoleChannel.hpp"
#include <mutex>

namespace Burst
{
	/**
	 * \brief Colors for the console.
	 */
	enum class ConsoleColor
	{
		Black = 0,
		Blue = 1,
		Green = 2,
		Cyan = 3,
		Red = 4,
		Magenta = 5,
		Brown = 6,
		LightGray = 7,
		DarkGray = 8,
		LightBlue = 9,
		LightGreen = 10,
		LightCyan = 11,
		LightRed = 12,
		LightMagenta = 13,
		Yellow = 14,
		White = 15
	};

	/**
	 * \brief A pair of fore- and background.
	 */
	struct ConsoleColorPair
	{
		ConsoleColor foreground, background;
	};

	/**
	 * \brief A print block for the console, that will block the output on the console when created
	 * and unlocks it on destruction of the object.
	 */
	class PrintBlock
	{
	public:
		/**
		 * \brief Constructor.
		 * Locks the console mutex.
		 * \param stream The output stream.
		 * \param mutex The console mutex.
		 */
		PrintBlock(std::ostream& stream, std::recursive_mutex& mutex);

		/**
		 * \brief Move-constructor.
		 * \param rhs The object to move.
		 */
		PrintBlock(PrintBlock&& rhs) noexcept;

		/**
		 * \brief Destructor.
		 * Unlocks the console mutex.
		 */
		~PrintBlock();

		/**
		 * \brief Prints a value to output.
		 * \tparam T 
		 * \param text 
		 * \return 
		 */
		template <typename T>
		const PrintBlock& operator<< (const T& text) const
		{
			if (finished_)
				return *this;

			*stream_ << text;
			return *this;
		}

		/**
		 * \brief Changes the current color of the font.
		 * \param color The font color.
		 * \return The print block instance.
		 */
		const PrintBlock& operator<< (ConsoleColor color) const;

		/**
		 * \brief Changes he current color of the font.
		 * \param color The font color.
		 * \return The print block instance.
		 */
		const PrintBlock& operator<< (ConsoleColorPair color) const;

		/**
		 * \brief Prints the current local time.
		 * The format will be HH:MM:SS.
		 * \return The print block instance.
		 */
		const PrintBlock& addTime() const;

		/**
		 * \brief Prints a line break.
		 * \return The print block instance.
		 */
		const PrintBlock& nextLine() const;

		/**
		 * \brief Resets the current line.
		 * \param wipe If true, the line will be erased,
		 * otherwise only the cursor will be set to the beginning of the line.
		 */
		const PrintBlock& clearLine(bool wipe = true) const;

		/**
		 * \brief Unlocks the console mutex.
		 * Every output that will be made with this object will not be discarded.
		 */
		void finish();

	private:
		std::ostream* stream_;
		std::recursive_mutex* mutex_;
		mutable std::mutex inner_mutex_;
		bool finished_;
	};

	/**
	 * \brief A static abstraction class for the console/terminal.
	 */
	class Console
	{
	public:
		/**
		 * \brief Changes the color of the font in the console.
		 * \param foreground The new foreground color.
		 * \param background The new background color.
		 */
		static void setColor(ConsoleColor foreground, ConsoleColor background = ConsoleColor::Black);

		/**
		 * \brief Changes the color of the font in the console.
		 * \param color The new back- and foreground color.
		 */
		static void setColor(ConsoleColorPair color);

		/**
		 * \brief Changes the color of the font in the console back to the default value.
		 */
		static void resetColor();

		/**
		 * \brief Returns an ANSI color escape code for the terminal.
		 * Only useful for Unix systems.
		 * \param color The color, for which the ANSI color escape code will be returned.
		 * \return The ANSI color escape code.
		 */
		static std::string getUnixConsoleCode(ConsoleColor color);

		/**
		 * \brief Creates a print block for the console for console output.
		 * \return The shared_ptr for a newly created print block.
		 */
		static std::shared_ptr<PrintBlock> print();

		/**
		 * \brief Resets the current line.
		 * \param wipe If true, the line will be erased,
		 * otherwise only the cursor will be set to the beginning of the line.
		 */
		static void clearLine(bool wipe = true);

	private:
		Console() = delete;
		~Console() = delete;

		static ConsoleColorPair currentColor_;
		static std::recursive_mutex mutex_;
	};
}
