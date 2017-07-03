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
#include <vector>
#include <Poco/Channel.h>
#include <Poco/Message.h>
#include <unordered_map>
#include <Poco/Logger.h>
#include <Poco/FormattingChannel.h>
#include <functional>
#include "MinerServer.hpp"
#include "Output.hpp"
#include <Poco/NotificationQueue.h>
#include "Message.hpp"

namespace Burst
{
	class BlockData;

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

		class ColoredPriorityConsoleChannel : public Poco::Channel
		{
		public:
			ColoredPriorityConsoleChannel();
			explicit ColoredPriorityConsoleChannel(Poco::Message::Priority priority);
			~ColoredPriorityConsoleChannel() override = default;

			void log(const Poco::Message& msg) override;
			void setPriority(Poco::Message::Priority priority);
			Poco::Message::Priority getPriority() const;

		private:
			static Poco::FastMutex mutex_;
			Poco::Message::Priority priority_;
		};

		class MinerDataChannel : public Poco::Channel
		{
		public:
			MinerDataChannel();
			explicit MinerDataChannel(MinerData* minerData);
			~MinerDataChannel() override = default;

			void log(const Poco::Message& msg) override;

			void setMinerData(MinerData* minerData);
			MinerData* getMinerData() const;

		private:
			MinerData* minerData_;
		};

		struct ChannelDefinition
		{
			ChannelDefinition(std::string name, Poco::Message::Priority default_priority);
			std::string name;
			Poco::Message::Priority default_priority;
		};

		static const std::vector<ChannelDefinition> channelDefinitions;
		
		static ColorPair getTextTypeColor(TextType type);

		static void writeProgress(float progress, size_t pipes);

		static void nextLine();
		static void setTextTypeColor(TextType type, ColorPair color);

		static void setup();

		/**
		 * \brief Refreshes all logging channels.
		 * Not used channels will be removed.
		 */
		static void refreshChannels();

		static bool setChannelPriority(const std::string& channel, Poco::Message::Priority priority);
		static bool setChannelPriority(const std::string& channel, const std::string& priority);
		static std::string getChannelPriority(const std::string& channel);
		static Poco::Message::Priority getStringToPriority(const std::string& priority);
		static std::string getPriorityToString(Poco::Message::Priority priority);
		static std::map<std::string, std::string> getChannelPriorities();
		static std::string setLogDir(const std::string& dir);
		static void setChannelMinerData(MinerData* minerData);
		static Poco::FormattingChannel* getFileFormattingChannel();

		static Poco::Logger* miner;
		static Poco::Logger* config;
		static Poco::Logger* server;
		static Poco::Logger* socket;
		static Poco::Logger* session;
		static Poco::Logger* nonceSubmitter;
		static Poco::Logger* plotReader;
		static Poco::Logger* plotVerifier;
		static Poco::Logger* wallet;
		static Poco::Logger* general;
		
		static void setOutput(Output id, bool set);
		static bool hasOutput(Output id);
		static const Output_Flags& getOutput();

	private:
		static void write(const std::string& text, TextType type = TextType::Normal);
		static void write(const std::vector<std::string>& lines, TextType type = TextType::Normal);
		static void writeStackframe(const std::string& additionalText);
		static void writeStackframe(std::vector<std::string>& additionalLines);

		static MinerLogger& getInstance();
		static void print(const std::string& text);
		static void writeAndFunc(TextType type, std::function<void()> func);
		static void setColor(Color foreground, Color background = Color::Black);
		static void setColor(ColorPair color);
		static void setColor(TextType type);
		static void clearLine(bool wipe = true);
		static void printTime();
		static void printProgress(float progress, size_t pipes);

		static std::mutex consoleMutex;
		static ColorPair currentColor;
		static TextType currentTextType;
		static std::map<TextType, ColorPair> typeColors;
		static bool progressFlag_;
		static float lastProgress_;
		static size_t lastPipeCount_;

		static const std::unordered_map<std::string, ColoredPriorityConsoleChannel*> channels_;
		static const std::unordered_map<std::string, MinerDataChannel*> websocketChannels_;
		static Output_Flags output_;
		static Poco::Channel* fileChannel_;
		static Poco::FormattingChannel* fileFormatter_;
		static std::string logFileName_;
	};

	
}
