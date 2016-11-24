//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#pragma once

#include <string>
#include <functional>
#include "SocketDefinitions.hpp"

namespace Burst
{
	class Miner;
	class Socket;

	class MinerProtocol
	{
	public:
		MinerProtocol();
		~MinerProtocol();

		bool run(Miner* miner);
		void stop();
		uint64_t getTargetDeadline() const;
		//SubmitResponse submitNonce(uint64_t nonce, uint64_t accountId, uint64_t& deadline);

	private:
		Miner* miner;
		bool running;
		bool getMiningInfo();
		uint64_t currentBlockHeight;
		uint64_t currentBaseTarget;
		uint64_t targetDeadline = 0u;
		std::string gensig;

		//MinerSocket miningInfoSocket;
		//MinerSocket nonceSubmitterSocket;
	};
}
