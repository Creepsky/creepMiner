//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#ifndef cryptoport_MinerLogger_h
#define cryptoport_MinerLogger_h
#include "Miner.h"

namespace Burst
{
    class MinerLogger
    {
    public:
        static void write(const std::string text);
        static MinerLogger* getInstance();
        std::mutex consoleMutex;
    private:
    };
}

#endif
