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
#include <sstream>
#include <Poco/NestedDiagnosticContext.h>
#include <functional>
#include "MinerServer.hpp"
#include "Output.hpp"

namespace Poco
{
	class Logger;
}

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
		static bool setChannelPriority(const std::string& channel, Poco::Message::Priority priority);
		static bool setChannelPriority(const std::string& channel, const std::string& priority);
		static std::string getChannelPriority(const std::string& channel);
		static Poco::Message::Priority getStringToPriority(const std::string& priority);
		static std::string getPriorityToString(Poco::Message::Priority priority);
		static std::map<std::string, std::string> getChannelPriorities();
		static std::string setLogDir(const std::string& dir);
		static void setChannelMinerData(MinerData* minerData);
		
		static Poco::Logger& miner;
		static Poco::Logger& config;
		static Poco::Logger& server;
		static Poco::Logger& socket;
		static Poco::Logger& session;
		static Poco::Logger& nonceSubmitter;
		static Poco::Logger& plotReader;
		static Poco::Logger& plotVerifier;
		static Poco::Logger& wallet;
		static Poco::Logger& general;
		
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

		static const std::unordered_map<std::string, ColoredPriorityConsoleChannel*> channels_;
		static const std::unordered_map<std::string, MinerDataChannel*> websocketChannels_;
		static Output_Flags output_;
		static Poco::Channel* fileChannel_;
		static Poco::FormattingChannel* fileFormatter_;
		static std::string logFileName_;
	};

	using TextType = MinerLogger::TextType;

	template <Poco::Message::Priority Priority, TextType Type>
	struct Message
	{
		/**
		 * \brief Logging a message.
		 * \param logger The logger, that logs the message.
		 * \param text The logged message.
		 * \param file The file, in which the log was created.
		 * \param line The line in the file, in which the log was created.
		 */
		Message(Poco::Logger& logger, const std::string& text, const char* file, int line, bool condition = true)
		{
			log(logger, text, file, line, condition);
		}

		/**
		 * \brief Logging a message with placeholders.
		 * \tparam Args Variadic types of the values for the placeholders.
		 * \param logger The logger, that logs the message.
		 * \param format The logged message including spaceholders.
		 * \param file The file, in which the log was created.
		 * \param line The line in the file, in which the log was created.
		 * \param args The values for the placeholders in format.
		 */
		template <typename ...Args>
		Message(Poco::Logger& logger, const std::string& format, const char* file, int line, bool condition, Args&&... args)
		{
			log(logger, Poco::format(format, args...), file, line, condition);
		}

		/**
		 * \brief Logging an exception.
		 * \param logger The logger, that logs the message.
		 * \param exception The exception being logged.
		 * \param file The file, in which the log was created.
		 * \param line The line in the file, in which the log was created.
		 */
		Message(Poco::Logger& logger, Poco::Exception& exception, const char* file, int line)
		{
			log(logger, Poco::format("Exception occured: %s\n\terror-code: %d\n\tclass: %s",
				exception.displayText(), exception.code(), std::string(exception.className())), file, line);
		}

		/**
		 * \brief Logging a stackframe.
		 * \param logger The logger, that logs the message.
		 * \param stackframe The stackframe, that is being logged.
		 * \param file The file, in which the log was created.
		 * \param line The line in the file, in which the log was created.
		 */
		Message(Poco::Logger& logger, const Poco::NestedDiagnosticContext& stackframe, const char* file, int line)
		{
			Message::stackframe(logger, stackframe, file, line);
		}

		/**
		 * \brief Logging a message with a dump of a memory block.
		 * \param logger The logger, that logs the message.
		 * \param text The additional logged message.
		 * \param memory The dumped memory block.
		 * \param size The size of the memory to dump.
		 * \param file The file, in which the log was created.
		 * \param line The line in the file, in which the log was created.
		 */
		Message(Poco::Logger& logger, const std::string& text, const void* memory, size_t size, const char* file, int line)
		{
			logger.dump(text, memory, size);
		}

	private:
		void log(Poco::Logger& logger, const std::string& text, const char* file, int line, bool condition = true) const
		{
			Poco::Message message;
			message.setText(text);
			message.setPriority(Priority);
			message.set("type", std::to_string(static_cast<int>(Type)));
			message.set("condition", std::to_string(condition));
			message.setSource(logger.name());
			message.setSourceFile(file);
			message.setSourceLine(line);
			logger.log(message);
		}

		void stackframe(Poco::Logger& logger, const Poco::NestedDiagnosticContext& stackframe, const char* file, int line) const
		{
			std::stringstream ss;
			ss << "Stackframe" << std::endl;
			stackframe.dump(ss);

			log(logger, ss.str(), file, line);
		}
	};

	template <TextType Type>
	using Fatal = Message<Poco::Message::Priority::PRIO_FATAL, Type>;

	template <TextType Type>
	using Critical = Message<Poco::Message::Priority::PRIO_CRITICAL, Type>;

	template <TextType Type>
	using Error = Message<Poco::Message::Priority::PRIO_ERROR, Type>;

	template <TextType Type>
	using Warning = Message<Poco::Message::Priority::PRIO_WARNING, Type>;

	template <TextType Type>
	using Notice = Message<Poco::Message::Priority::PRIO_NOTICE, Type>;

	template <TextType Type>
	using Information = Message<Poco::Message::Priority::PRIO_INFORMATION, Type>;

	template <TextType Type>
	using Debug = Message<Poco::Message::Priority::PRIO_DEBUG, Type>;

	template <TextType Type>
	using Trace = Message<Poco::Message::Priority::PRIO_TRACE, Type>;

#define log_fatal(logger, text, ...) Burst::Fatal<Burst::TextType::Error>(logger, text, __FILE__, __LINE__, true, ##__VA_ARGS__)
#define log_critical(logger, text, ...) Burst::Critical<Burst::TextType::Error>(logger, text, __FILE__, __LINE__, true, ##__VA_ARGS__)
#define log_error(logger, text, ...) Burst::Error<Burst::TextType::Error>(logger, text, __FILE__, __LINE__, true, ##__VA_ARGS__)
#define log_warning(logger, text, ...) Burst::Warning<Burst::TextType::Error>(logger, text, __FILE__, __LINE__, true, ##__VA_ARGS__)
#define log_notice(logger, text, ...) Burst::Notice<Burst::TextType::Information>(logger, text, __FILE__, __LINE__, true, ##__VA_ARGS__)
#define log_information(logger, text, ...) Burst::Information<Burst::TextType::Normal>(logger, text, __FILE__, __LINE__, true, ##__VA_ARGS__)
#define log_information_if(logger, cond, text, ...) Burst::Information<Burst::TextType::Normal>(logger, text, __FILE__, __LINE__, cond, ##__VA_ARGS__)
#define log_debug(logger, text, ...) Burst::Debug<Burst::TextType::Debug>(logger, text, __FILE__, __LINE__, true, ##__VA_ARGS__)
#define log_trace(logger, text, ...) Burst::Trace<Burst::TextType::Debug>(logger, text, __FILE__, __LINE__, true, ##__VA_ARGS__)
#define log_exception(logger, exception) Burst::Error<Burst::TextType::Error>(logger, exception, __FILE__, __LINE__)
#define log_memory(logger, text, memory, size) Burst::Trace<Burst::TextType::Debug>(logger, text, memory, size, __FILE__, __LINE__)
#define log_stackframe(logger, stackframe) Burst::Error<Burst::TextType::Error>(logger, stackframe, __FILE__, __LINE__)
#define log_current_stackframe(logger) log_stackframe(logger, Poco::NestedDiagnosticContext::current())

#define log_ok(logger, text, ...) Burst::Information<Burst::TextType::Ok>(logger, text, __FILE__, __LINE__, true, ##__VA_ARGS__)
#define log_ok_if(logger, cond, text, ...) Burst::Information<Burst::TextType::Ok>(logger, text, __FILE__, __LINE__, cond, ##__VA_ARGS__)
#define log_unimportant(logger, text, ...) Burst::Information<Burst::TextType::Unimportant>(logger, text, __FILE__, __LINE__, true, ##__VA_ARGS__)
#define log_unimportant_if(logger, cond, text, ...) Burst::Information<Burst::TextType::Unimportant>(logger, text, __FILE__, __LINE__, cond, ##__VA_ARGS__)
#define log_success(logger, text, ...) Burst::Information<Burst::TextType::Success>(logger, text, __FILE__, __LINE__, true, ##__VA_ARGS__)
#define log_success_if(logger, cond, text, ...) Burst::Information<Burst::TextType::Success>(logger, text, __FILE__, __LINE__, cond, ##__VA_ARGS__)
#define log_system(logger, text, ...) Burst::Information<Burst::TextType::System>(logger, text, __FILE__, __LINE__, true, ##__VA_ARGS__)
#define log_system_if(logger, cond, text, ...) Burst::Information<Burst::TextType::System>(logger, text, __FILE__, __LINE__, cond, ##__VA_ARGS__)

}
