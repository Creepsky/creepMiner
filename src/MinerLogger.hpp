//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#pragma once

#include <string>
#include <mutex>
#include <map>
#include <atomic>
#include <vector>

namespace Burst
{
	class MinerLogger
	{
	public:
		enum class Color
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

		struct ColorPair
		{
			Color foreground, background;
		};

		enum class TextType
		{
			Normal,
			Error,
			Information,
			Success,
			Warning,
			Important,
			System,
			Unimportant,
			Ok,
			Debug,
			Progress
		};

		static void write(const std::string& text, TextType type = TextType::Normal);
		static void write(const std::vector<std::string>& lines, TextType type = TextType::Normal);
		static ColorPair getTextTypeColor(TextType type);
		static void writeStackframe(const std::string& additionalText);
		static void writeStackframe(std::vector<std::string>& additionalLines);

		static void writeProgress(float progress, size_t pipes);

		static void nextLine();
		static void setTextTypeColor(TextType type, ColorPair color);

	private:
		static MinerLogger& getInstance();
		static void print(const std::string& text);
		static void writeAndFunc(Burst::MinerLogger::TextType type, std::function<void()> func);
		static void setColor(Color foreground, Color background = Color::Black);
		static void setColor(ColorPair color);
		static void setColor(TextType type);
		static void clearLine();
		static void printTime();
		static void printProgress(float progress, size_t pipes);

		static std::mutex consoleMutex;
		static ColorPair currentColor;
		static TextType currentTextType;
		static std::map<TextType, ColorPair> typeColors;
		static bool progressFlag_;
		static float lastProgress_;
		static size_t lastPipeCount_;
	};

	using TextType = MinerLogger::TextType;
}
