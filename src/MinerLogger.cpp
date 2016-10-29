//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#include "MinerLogger.h"
#include <iostream>
#include <iomanip>
#include <mutex>

void Burst::MinerLogger::write(const std::string& text)
{
    std::lock_guard<std::mutex> lock(getInstance().consoleMutex);
	
	auto now = std::chrono::system_clock::now();
	auto now_c = std::chrono::system_clock::to_time_t(now);

	std::cout << std::put_time(std::localtime(&now_c), "%X") << ": " << text << std::endl;
}

void Burst::MinerLogger::nextLine()
{
	std::lock_guard<std::mutex> lock(getInstance().consoleMutex);
	std::cout << std::endl;
}

Burst::MinerLogger& Burst::MinerLogger::getInstance()
{
    static MinerLogger instance;
    return instance;
}
