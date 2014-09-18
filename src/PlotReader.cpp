//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#include "Miner.h"

Burst::PlotReader::PlotReader(Miner* miner)
{
    this->done = true;
    this->miner = miner;
}

Burst::PlotReader::~PlotReader()
{
    this->stop();
}

void Burst::PlotReader::stop()
{
    this->done = true;
    if(readerThreadObj.joinable())
    {
        readerThreadObj.join();
    }
}

bool Burst::PlotReader::isDone() const
{
    return this->done;
}

void Burst::PlotReader::read(const std::string path)
{
    this->stop();
    this->accountId   = std::stoull(getAccountIdFromPlotFile(path));
    this->nonceStart  = std::stoull(Burst::getStartNonceFromPlotFile(path));
    this->nonceCount  = std::stoull(Burst::getNonceCountFromPlotFile(path));
    this->staggerSize = std::stoull(Burst::getStaggerSizeFromPlotFile(path));
    this->scoopNum    = this->miner->getScoopNum();
    this->gensig      = miner->getGensig();
    this->done        = false;
    this->inputPath   = path;
    this->readerThreadObj = std::thread(&PlotReader::readerThread,this);
}

void Burst::PlotReader::readerThread()
{
    std::ifstream inputStream(this->inputPath);
    if(inputStream.good())
    {
        inputStream.clear();
        
        this->buffer[0].resize(this->staggerSize);
        this->buffer[1].resize(this->staggerSize);
        this->readBuffer  = &this->buffer[0];
        this->writeBuffer = &this->buffer[1];
        
        size_t bufferSize  = this->staggerSize * MinerConfig::scoopSize;
        size_t startByte = scoopNum * MinerConfig::scoopSize * this->staggerSize;
        size_t chunkNum = 0;
        size_t totalChunk = this->nonceCount / this->staggerSize;
        this->nonceOffset = 0;

        this->runVerify = true;
        std::unique_lock<std::mutex> verifyLock(this->readLock);
        verifyLock.unlock();
        std::thread verifierThreadObj(&PlotReader::verifierThread,this);
        
        while(!this->done && inputStream.good() && chunkNum <= totalChunk)
        {
            inputStream.seekg(startByte + chunkNum*this->staggerSize*MinerConfig::plotSize);
            char* scoopData = (char*)&(*this->writeBuffer)[0];
            inputStream.read(scoopData, bufferSize);
            
            verifyLock.lock();
                std::vector<ScoopData>* temp = this->readBuffer;
                this->readBuffer  = this->writeBuffer;
                this->writeBuffer = temp;
                this->nonceOffset = chunkNum*this->staggerSize;
            verifyLock.unlock();
            this->readSignal.notify_all();
            
            chunkNum++;
        }
        inputStream.close();
        
        this->runVerify = false;
        verifyLock.lock();
        this->readSignal.notify_all();
        verifyLock.unlock();
        
        this->done = true;
        verifierThreadObj.join();
        MinerLogger::write("plot read done. "+this->inputPath+" "+std::to_string(this->nonceRead)+" nonces ");
    }
}

void Burst::PlotReader::verifierThread()
{
    std::unique_lock<std::mutex> verifyLock(this->readLock);
    this->nonceRead = 0;
    
    while(this->runVerify)
    {
        readSignal.wait(verifyLock);
        for(size_t i=0 ; i<this->readBuffer->size() ; i++)
        {
            if(this->nonceOffset + i < this->nonceCount)
            {
                HashData target;
                char* test = (char*)&((*this->readBuffer)[i]);
                this->hash.update(&this->gensig[0], MinerConfig::hashSize);
                this->hash.update(test,MinerConfig::scoopSize);
                this->hash.close(&target[0]);
                
                uint64_t targetResult = 0;
                memcpy(&targetResult,&target[0],sizeof(uint64_t));
                uint64_t deadline = targetResult / this->miner->getBaseTarget();
                
                uint64_t nonceNum = this->nonceStart + this->nonceOffset + i;
                this->miner->submitNonce(nonceNum, this->accountId, deadline);
                this->nonceRead++;
            }
        }
    }
}
