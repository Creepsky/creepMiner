//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#ifndef cryptoport_PlotReader_h
#define cryptoport_PlotReader_h
#include "Miner.h"

namespace Burst
{
    class PlotReader
    {
    public:
        PlotReader(Miner* miner);
        ~PlotReader();
        
        void read(const std::string path);
        void stop();
        bool isDone() const;
        
    private:
        void readerThread();
        void verifierThread();
        
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
        std::thread readerThreadObj;
        
        std::vector<ScoopData> buffer[2];
        
        Shabal256 hash;
        bool verifySignaled;
        std::mutex verifyMutex;
        std::condition_variable verifySignal;
        std::vector<ScoopData>* readBuffer;
        std::vector<ScoopData>* writeBuffer;
    };
}
#endif
