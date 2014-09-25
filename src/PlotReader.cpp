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
	std::ifstream inputStream(this->inputPath, std::ifstream::binary);
    if(inputStream.good())
    {
        /*
        inputStream.clear();
        this->runVerify = true;
        std::unique_lock<std::mutex> verifyLock(this->readLock);
        verifyLock.unlock();
        std::thread verifierThreadObj(&PlotReader::verifierThread,this);
        
        size_t readlimit = this->miner->getConfig()->maxBufferSizeMB / 32;
        size_t scoopBufferCount = this->staggerSize / readlimit;
        size_t scoopBufferSize = readlimit;
        size_t scoopDoneRead = 0;
        while(scoopDoneRead <= scoopBufferCount)
        {
            size_t staggerOffset = this->scoopNum * MinerConfig::scoopSize * scoopDoneRead * scoopBufferSize;
            if(scoopBufferSize > (this->staggerSize - (scoopDoneRead * scoopBufferSize)))
            {
                scoopBufferSize = this->staggerSize - (scoopDoneRead * scoopBufferSize);
            }
            
            this->buffer[0].resize(scoopBufferSize);
            this->buffer[1].resize(scoopBufferSize);
            this->readBuffer  = &this->buffer[0];
            this->writeBuffer = &this->buffer[1];
            
            size_t bufferSize  = scoopBufferSize * MinerConfig::scoopSize;
            size_t startByte = this->scoopNum * MinerConfig::scoopSize * scoopBufferSize + staggerOffset;
            size_t chunkNum = 0;
            size_t totalChunk = this->nonceCount / scoopBufferSize;
            this->nonceOffset = 0;
            
            while(!this->done && inputStream.good() && chunkNum <= totalChunk)
            {
                inputStream.seekg(startByte + chunkNum*scoopBufferSize*MinerConfig::plotSize);
                char* scoopData = (char*)&(*this->writeBuffer)[0];
                inputStream.read(scoopData, bufferSize);
                
                verifyLock.lock();
                std::vector<ScoopData>* temp = this->readBuffer;
                this->readBuffer  = this->writeBuffer;
                this->writeBuffer = temp;
                this->nonceOffset = chunkNum*scoopBufferSize;
                verifyLock.unlock();
                this->readSignal.notify_all();
                
                chunkNum++;
            }
            
            scoopDoneRead++;
        }
        */
        
        /*
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
        */
        
        this->runVerify = true;
        std::thread verifierThreadObj(&PlotReader::verifierThread,this);
        
        size_t chunkNum = 0;
        size_t totalChunk = (size_t)std::ceil((double)this->nonceCount / (double)this->staggerSize);
        this->nonceOffset = 0;
        this->nonceRead = 0;
        this->verifySignaled = false;
        
        this->readBuffer  = &this->buffer[0];
        this->writeBuffer = &this->buffer[1];
        
        while(!this->done && inputStream.good() && chunkNum <= totalChunk)
        {
            size_t scoopBufferSize = this->miner->getConfig()->maxBufferSizeMB*1024*1024 / (64*2);
            size_t scoopBufferCount = (size_t)std::ceil((float)(this->staggerSize*MinerConfig::scoopSize) / (float)(scoopBufferSize));
            size_t startByte = this->scoopNum * MinerConfig::scoopSize*this->staggerSize + chunkNum*this->staggerSize*MinerConfig::plotSize;
            size_t scoopDoneRead = 0;
            size_t staggerOffset = 0;
            
            while(!this->done && inputStream.good() && scoopDoneRead <= scoopBufferCount)
            {
                this->writeBuffer->resize(scoopBufferSize / MinerConfig::scoopSize);
                staggerOffset = scoopDoneRead * scoopBufferSize;
                if(scoopBufferSize > (this->staggerSize*MinerConfig::scoopSize - (scoopDoneRead * scoopBufferSize)))
                {
                    scoopBufferSize = this->staggerSize*MinerConfig::scoopSize - (scoopDoneRead * scoopBufferSize);
                    if(scoopBufferSize > MinerConfig::scoopSize)
                    {
                        this->writeBuffer->resize(scoopBufferSize / MinerConfig::scoopSize);
                    }
                }
                
                if(scoopBufferSize > MinerConfig::scoopSize)
                {
                    inputStream.seekg(startByte + staggerOffset);
                    char* scoopData = (char*)&(*this->writeBuffer)[0];
                    inputStream.read(scoopData, scoopBufferSize);
                    
                    
                    std::unique_lock<std::mutex> verifyLock(this->verifyMutex);
                    
                    //MinerLogger::write("chunk "+std::to_string(chunkNum)+" offset "+std::to_string(startByte + staggerOffset)+" read "+std::to_string(scoopBufferSize)+" nonce offset "+std::to_string(this->nonceOffset)+" nonceRead "+std::to_string(this->nonceRead));
                    
                    std::vector<ScoopData>* temp = this->readBuffer;
                    this->readBuffer  = this->writeBuffer;
                    this->writeBuffer = temp;
                    this->nonceOffset = chunkNum*this->staggerSize + scoopDoneRead*(scoopBufferSize / MinerConfig::scoopSize);
                    this->verifySignaled = true;
                    this->verifySignal.notify_one();
                    verifyLock.unlock();
                    
                    while(this->verifySignaled)
                    {
                        //MinerLogger::write("stupid");
                        std::this_thread::sleep_for(std::chrono::microseconds(1));
                        this->verifySignal.notify_one();
                    };
                    
                }
                /*
                else
                {
                    MinerLogger::write("scoop buffer ="+std::to_string(scoopBufferSize));
                }
                 */
                scoopDoneRead++;
            }
            
            chunkNum++;
        }
        
        inputStream.close();

        
        std::unique_lock<std::mutex> verifyLock(this->verifyMutex);
        this->runVerify = false;
        this->readBuffer->clear();
        this->writeBuffer->clear();
        this->verifySignaled = true;
        this->verifySignal.notify_all();
        verifyLock.unlock();
        
        verifierThreadObj.join();
        
        this->done = true;
        MinerLogger::write("plot read done. "+Burst::getFileNameFromPath(this->inputPath)+" = "+std::to_string(this->nonceRead)+" nonces ");
    }
}

void Burst::PlotReader::verifierThread()
{
    std::unique_lock<std::mutex> verifyLock(this->verifyMutex);
    
    while(this->runVerify)
    {
        do {
            this->verifySignal.wait(verifyLock);
        }
        while(!this->verifySignaled);
        this->verifySignaled = false;
        
        for(size_t i=0 ; i<this->readBuffer->size() ; i++)
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
        //MinerLogger::write("verifier processed "+std::to_string(this->nonceRead)+" readsize "+std::to_string(this->readBuffer->size()));
    }
    //MinerLogger::write("plot read done. "+std::to_string(this->nonceRead)+" nonces ");
}
