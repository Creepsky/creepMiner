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

#include <Poco/Message.h>
#include <Poco/Logger.h>
#include <Poco/NestedDiagnosticContext.h>
#include <Poco/NotificationQueue.h>
#include <thread>
#include <Poco/Task.h>

namespace Burst
{
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

	/**
	 * \brief A Notification that holds a message to be send.
	 */
	struct MessageNotification : Poco::Notification
	{
		/**
		 * \brief short alias for Poco::AutoPtr<MessageNotification>
		 */
		using Ptr = Poco::AutoPtr<MessageNotification>;

		/**
		 * \brief Constructor.
		 * \param logger The sending logger.
		 * \param message The message to send.
		 */
		MessageNotification(Poco::Logger& logger, Poco::Message message);

		/**
		 * \brief The message that was sent.
		 */
		Poco::Message message;

		/**
		 * \brief The sending logger.
		 */
		Poco::Logger* logger;
	};

	struct Message
	{
		/**
		* \brief Logging a message.
		* \param priority The priority of the message.
		* \param type The type of the message.
		* \param logger The logger, that logs the message.
		* \param text The logged message.
		* \param file The file, in which the log was created.
		* \param line The line in the file, in which the log was created.
		*/
		static void log(Poco::Message::Priority priority, TextType type,
		                Poco::Logger& logger, const std::string& text, const char* file, int line, bool condition = true);

		/**
		* \brief Logging a message with placeholders.
		* \tparam Args Variadic types of the values for the placeholders.
		* \param priority The priority of the message.
		* \param type The type of the message.
		* \param logger The logger, that logs the message.
		* \param format The logged message including spaceholders.
		* \param file The file, in which the log was created.
		* \param line The line in the file, in which the log was created.
		* \param args The values for the placeholders in format.
		*/
		template <typename ...Args>
		static void log(Poco::Message::Priority priority, TextType type,
		                Poco::Logger& logger, const std::string& format, const char* file, int line, bool condition, Args&&... args)
		{
			log(priority, type, logger, Poco::format(format, args...), file, line, condition);
		}

		/**
		* \brief Logging an exception.
		* \param priority The priority of the message.
		* \param type The type of the message.
		* \param logger The logger, that logs the message.
		* \param exception The exception being logged.
		* \param file The file, in which the log was created.
		* \param line The line in the file, in which the log was created.
		*/
		static void log(Poco::Message::Priority priority, TextType type,
		                Poco::Logger& logger, Poco::Exception& exception, const char* file, int line);

		/**
		* \brief Logging a stackframe.
		* \param priority The priority of the message.
		* \param type The type of the message.
		* \param logger The logger, that logs the message.
		* \param stackframe The stackframe, that is being logged.
		* \param file The file, in which the log was created.
		* \param line The line in the file, in which the log was created.
		*/
		static void log(Poco::Message::Priority priority, TextType type,
		                Poco::Logger& logger, const Poco::NestedDiagnosticContext& stackframe, const char* file, int line);

		/**
		* \brief Logging a message with a dump of a memory block.
		* \param priority The priority of the message.
		* \param type The type of the message.
		* \param logger The logger, that logs the message.
		* \param text The additional logged message.
		* \param memory The dumped memory block.
		* \param size The size of the memory to dump.
		* \param file The file, in which the log was created.
		* \param line The line in the file, in which the log was created.
		*/
		static void log(Poco::Message::Priority priority, TextType type,
		                Poco::Logger& logger, const std::string& text, const void* memory, size_t size, const char* file, int line);

		/**
		 * \brief Logging a message into the logfile (if active).
		 * \param priority The priority of the message.
		 * \param type The type of the message.
		 * \param logger The logger, that logs the message.
		 * \param text The logged message.
		 * \param file The file, in which the log was created.
		*  \param line The line in the file, in which the log was created.
		 */
		static void logIntoFile(Poco::Message::Priority priority, TextType type,
		                        Poco::Logger& logger, const std::string& text, const char* file, int line);

		/**
		* \brief Logging a message into the logfile (if active) with placeholders.
		* \tparam Args Variadic types of the values for the placeholders.
		* \param priority The priority of the message.
		* \param type The type of the message.
		* \param logger The logger, that logs the message.
		* \param format The logged message including spaceholders.
		* \param file The file, in which the log was created.
		* \param line The line in the file, in which the log was created.
		* \param args The values for the placeholders in format.
		*/
		template <typename ...Args>
		static void logIntoFile(Poco::Message::Priority priority, TextType type,
		                        Poco::Logger& logger, const std::string& format, const char* file, int line, Args&&... args)
		{
			logIntoFile(priority, type, logger, Poco::format(format, args...), file, line);
		}

		/**
		 * \brief Wakes up all active dispatcher and forces them to terminate.
		 */
		static void wakeUpAllDispatcher();

		/**
		 * \brief An asyncronous message dispatcher.
		 * It logs every message, that will be pushed on a queue.
		 */
		struct Dispatcher : Poco::Task
		{
			/**
			 * \brief Constructor.
			 * \param queue The queue, from which the message are taken for logging.
			 */
			explicit Dispatcher(Poco::NotificationQueue& queue);

			/**
			 * \brief Runs the dispatcher.
			 */
			void runTask() override;

			/**
			 * \brief Creates a new dispatcher that access the global messages queue.
			 * \return A new Dispatcher.
			 */
			static std::unique_ptr<Dispatcher> create();

		private:
			// The message queue, from which the dispatcher takes messages for logging.
			Poco::NotificationQueue* queue_;
		};

	private:
		/**
		 * \brief Creates a message object.
		 * \param priority The priority of the message.
		 * \param type The type of the message.
		 * \param logger The logger, that created the message.
		 * \param text The content of the message.
		 * \param file The file, that created the message.
		 * \param line The line in the file, in that the message was created.
		 * \param condition Indicates, wether the message should be displayed or not.
		 * \return A Poco::Message object.
		 */
		static Poco::Message create(Poco::Message::Priority priority, TextType type,
		                            Poco::Logger& logger, const std::string& text, const char* file, int line, bool condition = true);

		/**
		 * \brief Logs a message on all connected and available channels.
		 * \param logger The logger, that will log the message.
		 * \param message The message that is logged.
		 */
		static void log(Poco::Logger& logger, const Poco::Message& message);

		/**
		 * \brief Creates and logs a message.
		 * \param priority The priority of the message.
		 * \param type The type of the message.
		 * \param logger The logger, that created the message.
		 * \param text The content of the message.
		 * \param file The file, that created the message.
		 * \param line The line in the file, in that the message was created.
		 * \param condition Indicates, wether the message should be displayed or not.
		 */
		static void createAndLog(Poco::Message::Priority priority, TextType type,
		                         Poco::Logger& logger, const std::string& text, const char* file, int line, bool condition = true);

		/**
		 * \brief Creates a messages with a stackframe as the content and logs it.
		 * \param priority The priority of the message.
		 * \param type The type of the message.
		 * \param logger The logger, that created the message.
		 * \param stackframe The stackframe to log.
		 * \param file The file, that created the message.
		 * \param line The line in the file, in that the message was created.
		 */
		static void stackframe(Poco::Message::Priority priority, TextType type,
		                       Poco::Logger& logger, const Poco::NestedDiagnosticContext& stackframe, const char* file, int line);

		/**
		 * \brief Constructor.
		 * Should not be called.
		 */
		Message();

		// queue for all messages to log
		static Poco::NotificationQueue messageQueue_;
	};
}

#define log_fatal(logger, text, ...) Burst::Message::log(Poco::Message::Priority::PRIO_FATAL, Burst::TextType::Error, *logger, text, __FILE__, __LINE__, true, ##__VA_ARGS__)
#define log_critical(logger, text, ...) Burst::Message::log(Poco::Message::Priority::PRIO_CRITICAL, Burst::TextType::Error, *logger, text, __FILE__, __LINE__, true, ##__VA_ARGS__)
#define log_error(logger, text, ...) Burst::Message::log(Poco::Message::Priority::PRIO_ERROR, Burst::TextType::Error, *logger, text, __FILE__, __LINE__, true, ##__VA_ARGS__)
#define log_warning(logger, text, ...) Burst::Message::log(Poco::Message::Priority::PRIO_WARNING, Burst::TextType::Error, *logger, text, __FILE__, __LINE__, true, ##__VA_ARGS__)
#define log_notice(logger, text, ...) Burst::Message::log(Poco::Message::Priority::PRIO_NOTICE, Burst::TextType::Information, *logger, text, __FILE__, __LINE__, true, ##__VA_ARGS__)
#define log_information(logger, text, ...) Burst::Message::log(Poco::Message::Priority::PRIO_INFORMATION, Burst::TextType::Normal, *logger, text, __FILE__, __LINE__, true, ##__VA_ARGS__)
#define log_information_if(logger, cond, text, ...) Burst::Message::log(Poco::Message::Priority::PRIO_INFORMATION, Burst::TextType::Normal, *logger, text, __FILE__, __LINE__, cond, ##__VA_ARGS__)
#define log_debug(logger, text, ...) Burst::Message::log(Poco::Message::Priority::PRIO_DEBUG, Burst::TextType::Debug, *logger, text, __FILE__, __LINE__, true, ##__VA_ARGS__)
#define log_trace(logger, text, ...) Burst::Message::log(Poco::Message::Priority::PRIO_TRACE, Burst::TextType::Debug, *logger, text, __FILE__, __LINE__, true, ##__VA_ARGS__)
#define log_exception(logger, exception) Burst::Message::log(Poco::Message::Priority::PRIO_ERROR, Burst::TextType::Error, *logger, exception, __FILE__, __LINE__)
#define log_memory(logger, text, memory, size) Burst::Message::log(Poco::Message::Priority::PRIO_TRACE, Burst::TextType::Debug, *logger, text, memory, size, __FILE__, __LINE__)
#define log_stackframe(logger, stackframe) Burst::Message::log(Poco::Message::Priority::PRIO_ERROR, Burst::TextType::Error, *logger, stackframe, __FILE__, __LINE__)
#define log_current_stackframe(logger) log_stackframe(logger, Poco::NestedDiagnosticContext::current())

#define log_ok(logger, text, ...) Burst::Message::log(Poco::Message::Priority::PRIO_INFORMATION, Burst::TextType::Ok, *logger, text, __FILE__, __LINE__, true, ##__VA_ARGS__)
#define log_ok_if(logger, cond, text, ...) Burst::Message::log(Poco::Message::Priority::PRIO_INFORMATION, Burst::TextType::Ok, *logger, text, __FILE__, __LINE__, cond, ##__VA_ARGS__)
#define log_unimportant(logger, text, ...) Burst::Message::log(Poco::Message::Priority::PRIO_INFORMATION, Burst::TextType::Unimportant, *logger, text, __FILE__, __LINE__, true, ##__VA_ARGS__)
#define log_unimportant_if(logger, cond, text, ...) Burst::Message::log(Poco::Message::Priority::PRIO_INFORMATION, Burst::TextType::Unimportant, *logger, text, __FILE__, __LINE__, cond, ##__VA_ARGS__)
#define log_success(logger, text, ...) Burst::Message::log(Poco::Message::Priority::PRIO_INFORMATION, Burst::TextType::Success, *logger, text, __FILE__, __LINE__, true, ##__VA_ARGS__)
#define log_success_if(logger, cond, text, ...) Burst::Message::log(Poco::Message::Priority::PRIO_INFORMATION, Burst::TextType::Success, *logger, text, __FILE__, __LINE__, cond, ##__VA_ARGS__)

#define log_system(logger, text, ...) Burst::Message::log(Poco::Message::Priority::PRIO_INFORMATION, Burst::TextType::System, *logger, text, __FILE__, __LINE__, true, ##__VA_ARGS__)
#define log_system_if(logger, cond, text, ...) Burst::Message::log(Poco::Message::Priority::PRIO_INFORMATION, Burst::TextType::System, *logger, text, __FILE__, __LINE__, cond, ##__VA_ARGS__)

#define log_file_only(logger, priority, type, text, ...) Burst::Message::logIntoFile(priority, type, *logger, text, __FILE__, __LINE__, ##__VA_ARGS__)
