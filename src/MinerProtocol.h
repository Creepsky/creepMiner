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

	class MinerSocket
	{
	public:
		void setRemote(const std::string& ip, size_t port, size_t defaultTimeout = 60);
		std::string httpPost(const std::string& url, const std::string& body, const std::string& header = "");
		std::string httpGet(const std::string& url);
		void httpPostAsync(const std::string& url, const std::string& body, std::function<void (std::string)> responseCallback);
		void httpGetAsync(const std::string& url, std::function<void (std::string)> responseCallback);

	private:
		std::string httpRequest(const std::string& method, const std::string& url,
								const std::string& body, const std::string& header);
		void httpRequestAsync(const std::string& method, const std::string& url,
							  const std::string& body, const std::string& header,
							  std::function<void (std::string)> responseCallback);
		static const size_t readBufferSize = 2048;
		char readBuffer[readBufferSize];
		struct sockaddr_in remoteAddr;
		struct timeval socketTimeout;
	};

	enum class SubmitResponse
	{
		Submitted,
		TooHigh,
		Error,
		None
	};

	class MinerProtocol
	{
	public:
		MinerProtocol();
		~MinerProtocol();

		bool run(Miner* miner);
		void stop();
		SubmitResponse submitNonce(uint64_t nonce, uint64_t accountId, uint64_t& deadline);

	private:
		Miner* miner;
		bool running;
		void getMiningInfo();
		uint64_t currentBlockHeight;
		uint64_t currentBaseTarget;
		uint64_t targetDeadline = 0u;
		std::string gensig;

		MinerSocket miningInfoSocket;
		MinerSocket nonceSubmitterSocket;
	};
}
