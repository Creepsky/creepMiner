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
#include "Output.hpp"
#include "Message.hpp"
#include "Console.hpp"
#include "ProgressPrinter.hpp"

namespace Burst
{
	class ColoredPriorityConsoleChannel;
	class MinerDataChannel;
	class BlockData;
	class MinerData;

	class MinerLogger
	{
	public:
		/**
		 * \brief Initializes the logger.
		 * When called more then once, the old context will be overwritten,
		 * so be careful!
		 */
		static void setup();

		struct ChannelDefinition
		{
			ChannelDefinition(std::string name, Poco::Message::Priority default_priority);
			std::string name;
			Poco::Message::Priority default_priority;
		};

		/**
		 * \brief Definitions of all logger.
		 */
		static const std::vector<ChannelDefinition> channelDefinitions;

		static ConsoleColorPair getTextTypeColor(TextType type);
		static void setTextTypeColor(TextType type, ConsoleColorPair color);

		/**
		 * \brief Writes a simple progress bar into stdcout.
		 * \param progress The procentual progress value (0-100).
		 */
		static void writeProgress(float progress);

		/**
		 * \brief Writes a text of a specifig type and a timestamp into stdout.
		 * The type can be splitted by \n.
		 * \param text The text to write.
		 * \param type The type of the text.
		 */
		static void write(const std::string& text, TextType type = TextType::Normal);

		/**
		 * \brief Writes a stack frame into stdout.
		 * \param additionalText An additional text, that will be displayed before the stack frame.
		 * Use it to explain, when and where the error happened.
		 */
		static void writeStackframe(const std::string& additionalText);

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
		
		/**
		 * \brief Sets the flag for a specific output.
		 * \param id The id of the output.
		 * \param set The flag for the output, that indicates, if the output should
		 * be visible or not.
		 */
		static void setOutput(Output id, bool set);

		/**
		 * \brief Returns the flag for a specifig output.
		 * \param id The id of the output.
		 * \return If true, the output should be shown, otherwise not.
		 */
		static bool hasOutput(Output id);

		/**
		 * \brief Returns the flags for all outputs.
		 * \return A flag-map for all outputs.
		 */
		static const Output_Flags& getOutput();

	private:		
		static std::recursive_mutex mutex_;
		static TextType currentTextType_;
		static std::map<TextType, ConsoleColorPair> typeColors;
		static bool progressFlag_;
		static float lastProgress_;
		static size_t lastPipeCount_;

		static const std::unordered_map<std::string, ColoredPriorityConsoleChannel*> channels_;
		static const std::unordered_map<std::string, MinerDataChannel*> websocketChannels_;
		static Output_Flags output_;
		static Poco::Channel* fileChannel_;
		static Poco::FormattingChannel* fileFormatter_;
		static std::string logFileName_;
		static ProgressPrinter progressPrinter_;
	};
}
