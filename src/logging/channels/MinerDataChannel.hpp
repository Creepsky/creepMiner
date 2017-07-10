#pragma once

#include <Poco/Channel.h>

namespace Burst
{
	class MinerData;

	/**
	 * \brief A logging channel that logs messages into the \class MinerData
	 */
	class MinerDataChannel : public Poco::Channel
	{
	public:
		/**
		 * \brief Constructor.
		 * The miner data is not defined, therefore no messages will be logged.
		 */
		MinerDataChannel();

		/**
		 * \brief Constructor.
		 * \param minerData The miner data, in which the channel will log messages.
		 */
		explicit MinerDataChannel(MinerData* minerData);

		/**
		 * \brief Destructor.
		 */
		~MinerDataChannel() override = default;

		/**
		 * \brief Logs a messages.
		 * When no miner data is defined, the message is discarded.
		 * \param msg The message.
		 */
		void log(const Poco::Message& msg) override;

		/**
		 * \brief Sets the destination miner data, in which the channel will log the messages.
		 * \param minerData The miner data.
		 */
		void setMinerData(MinerData* minerData);

		/**
		 * \brief Returns the miner data, in which the channel logs messages.
		 * \return The miner data.
		 */
		MinerData* getMinerData() const;

	private:
		MinerData* minerData_;
	};
}
