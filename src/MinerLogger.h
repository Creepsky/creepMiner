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

namespace Burst
{
    class MinerLogger
    {
    public:
        static void write(const std::string& text);
		static void nextLine();

		static MinerLogger& getInstance();

    private:
        std::mutex consoleMutex;
    };
}
