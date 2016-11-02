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
			Debug
		};

		static void write(const std::string& text, TextType type = TextType::Normal);

		static void nextLine();

		static void setTextTypeColor(TextType type, ColorPair color);
		static ColorPair getTextTypeColor(TextType type);

	private:
		static MinerLogger& getInstance();
		static void print(const std::string& text);
		static void setColor(Color foreground, Color background = Color::Black);
		static void setColor(ColorPair color);

		static std::mutex consoleMutex;
		static ColorPair currentColor;
		static std::map<TextType, ColorPair> typeColors;
	};

	using TextType = MinerLogger::TextType;
}
