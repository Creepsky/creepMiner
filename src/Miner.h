//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#pragma once

#include <mutex>
#include <unordered_map>
#include "MinerShabal.h"
#include "Declarations.hpp"
#include "MinerProtocol.h"

namespace Burst
{    
	class PlotReader;
	class MinerConfig;

    class Miner
    {
    public:
	    explicit Miner(MinerConfig& config);
        void run();
        size_t getScoopNum() const;
        uint64_t getBaseTarget() const;
        const GensigData& getGensig() const;
        const MinerConfig* getConfig() const;
        void updateGensig(const std::string gensigStr, uint64_t blockHeight, uint64_t baseTarget);
        void submitNonce(uint64_t nonce, uint64_t accountId, uint64_t deadline);
        void nonceSubmitReport(uint64_t nonce, uint64_t accountId, uint64_t deadline);

    private:
        void nonceSubmitterThread();
        bool running;
        MinerConfig* config;
        size_t scoopNum;
        uint64_t baseTarget;
        uint64_t blockHeight;
        std::string gensigStr;
        MinerProtocol protocol;
        Shabal256 hash;
        GensigData gensig;
		std::vector<std::shared_ptr<PlotReader>> plotReaders;
        std::unordered_map<uint64_t,uint64_t> bestDeadline;
        std::unordered_map<uint64_t,uint64_t> bestNonce;
        std::unordered_map<uint64_t,uint64_t> bestDeadlineConfirmed;
        std::mutex accountLock;
        std::chrono::system_clock::time_point nextNonceSubmission;
    };
}
