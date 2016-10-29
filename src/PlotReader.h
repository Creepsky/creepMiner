//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include "MinerShabal.h"
#include <mutex>
#include "Declarations.hpp"

namespace Burst
{
	class Miner;
	class PlotFile;

    class PlotReader
    {
    public:
		PlotReader() = default;
	    explicit PlotReader(Miner& miner);
        ~PlotReader();
        
        void read(const std::string& path);
		void read(std::vector<std::shared_ptr<PlotFile>>&& plotFiles);
        void stop();
        bool isDone() const;
        
    private:
        void readerThread();
        void verifierThread();
		void listReaderThread();

        size_t nonceStart;
        size_t scoopNum;
        size_t nonceCount;
        size_t nonceOffset;
        size_t nonceRead;
        size_t staggerSize;
        uint64_t accountId;
        GensigData gensig;
        
        bool done;
        bool runVerify;
        std::string inputPath;

        Miner* miner;
		std::vector<std::shared_ptr<PlotFile>> plotFileList;
        std::thread readerThreadObj;
        
        std::vector<ScoopData> buffer[2];
        
        Shabal256 hash;
        bool verifySignaled;
        std::mutex verifyMutex;
        std::condition_variable verifySignal;
        std::vector<ScoopData>* readBuffer;
        std::vector<ScoopData>* writeBuffer;
    };

	class PlotListReader
	{
	public:
		void read(std::vector<std::shared_ptr<PlotFile>>&& plotFiles);

	private:
		std::vector<std::shared_ptr<PlotFile>> plotFileList;
	};
}
