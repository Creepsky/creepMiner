#pragma once

#include <Poco/Message.h>
#include <Poco/Mutex.h>
#include <Poco/Channel.h>

namespace Burst
{
	/**
	 * \brief A logging channel that logs messages into the \class Console
	 */
	class ColoredPriorityConsoleChannel : public Poco::Channel
	{
	public:
		/**
		 * \brief Constructor.
		 * The default priority INFORMATION is set.
		 */
		ColoredPriorityConsoleChannel();

		/**
		 * \brief Constructor.
		 * \param priority The priority of the channel.
		 */
		explicit ColoredPriorityConsoleChannel(Poco::Message::Priority priority);

		/**
		 * \brief Destructor.
		 */
		~ColoredPriorityConsoleChannel() override = default;

		/**
		 * \brief Logs a messages.
		 * \param msg The message.
		 */
		void log(const Poco::Message& msg) override;

		/**
		 * \brief Sets the priority of the channel.
		 * \param priority The new priority.
		 */
		void setPriority(Poco::Message::Priority priority);

		/**
		 * \brief Returns the priority of the channel.
		 * \return The priority.
		 */
		Poco::Message::Priority getPriority() const;

	private:
		static Poco::FastMutex mutex_;
		Poco::Message::Priority priority_;
	};
}
