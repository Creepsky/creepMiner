//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#include "Miner.h"

Burst::Miner::Miner(MinerConfig& config)
{
    this->config = &config;
    this->running = false;
}

void Burst::Miner::run()
{
    this->running = true;
    std::thread submitter(&Miner::nonceSubmitterThread,this);
    if(!this->protocol.run(this))
    {
        MinerLogger::write("Mining networking failed");
    }
    this->running = false;
    submitter.join();
}

const Burst::MinerConfig* Burst::Miner::getConfig() const
{
    return this->config;
}

void Burst::Miner::updateGensig(const std::string gensigStr, uint64_t blockHeight, uint64_t baseTarget)
{
    this->gensigStr = gensigStr;
    this->blockHeight = blockHeight;
    this->baseTarget = baseTarget;
    for(size_t i=0 ; i<32 ; i++)
    {
        std::string byteStr = gensigStr.substr(i*2,2);
        this->gensig[i] = (uint8_t)std::stoi(byteStr,0,16);
    }
    
    GensigData newGenSig;
    this->hash.update(&this->gensig[0],this->gensig.size());
    this->hash.update(blockHeight);
	this->hash.close(&newGenSig[0]);
	this->scoopNum = ((int)(newGenSig[newGenSig.size()-2] & 0x0F) << 8) | (int)newGenSig[newGenSig.size()-1];
    
    MinerLogger::write("#"+std::to_string(blockHeight)+" "+
                       "["+std::to_string(this->baseTarget)+"] "+
                       this->gensigStr+
                       " scoop:"+std::to_string(this->scoopNum));
    
    this->bestDeadline.clear();
    this->config->rescan();
    
    this->plotReaders.clear();
    for(const std::string& path : this->config->plotList)
    {
        this->plotReaders.push_back(std::unique_ptr<PlotReader>(new PlotReader(this)));
        this->plotReaders[this->plotReaders.size()-1]->read(path);
    }
    
    std::default_random_engine randomizer((std::random_device())());
    std::uniform_int_distribution<size_t> dist(0, this->config->submissionMaxDelay);
    size_t delay = dist(randomizer);
    this->nextNonceSubmission = std::chrono::system_clock::now() +
                                std::chrono::seconds(delay);
    
    std::lock_guard<std::mutex> mutex(this->accountLock);
    this->bestDeadline.clear();
    this->bestNonce.clear();
    this->bestDeadlineConfirmed.clear();
}

const Burst::GensigData& Burst::Miner::getGensig() const
{
    return this->gensig;
}

uint64_t Burst::Miner::getBaseTarget() const
{
    return this->baseTarget;
}

size_t Burst::Miner::getScoopNum() const
{
    return this->scoopNum;
}

void Burst::Miner::nonceSubmitterThread()
{
    std::deque<std::pair<uint64_t,uint64_t>> submitList;
    std::unique_lock<std::mutex> mutex(this->accountLock);
    mutex.unlock();
    while(this->running)
    {
        if(std::chrono::system_clock::now() > this->nextNonceSubmission)
        {
            mutex.lock();
            for (auto& kv : this->bestDeadline)
            {
                uint64_t accountId = kv.first;
                uint64_t deadline = kv.second;
                uint64_t nonce = this->bestNonce[accountId];
                bool firstSubmit = this->bestDeadlineConfirmed.find(accountId) == this->bestDeadlineConfirmed.end();
                
                if( firstSubmit || deadline < this->bestDeadlineConfirmed[accountId] )
                {
                    submitList.push_back(std::make_pair(nonce, accountId));
                }
            }
            mutex.unlock();
            
            std::default_random_engine randomizer((std::random_device())());
            std::uniform_int_distribution<size_t> dist(0, this->config->submissionMaxDelay);
            this->nextNonceSubmission = std::chrono::system_clock::now() +
            std::chrono::seconds(dist(randomizer));
            
            if(submitList.size() > 0)
            {
                auto submitData = submitList.front();
                uint64_t confirmedDeadline = this->protocol.submitNonce(submitData.first, submitData.second);
                mutex.lock();
                if(this->bestDeadlineConfirmed.find(submitData.second) != this->bestDeadlineConfirmed.end())
                {
                    if(confirmedDeadline < this->bestDeadlineConfirmed[submitData.second])
                    {
                        this->bestDeadlineConfirmed[submitData.second] = confirmedDeadline;
                    }
                }
                else
                {
                    this->bestDeadlineConfirmed.insert(std::make_pair(submitData.second, confirmedDeadline));
                }
                mutex.unlock();
                submitList.pop_front();
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void Burst::Miner::submitNonce(uint64_t nonce, uint64_t accountId, uint64_t deadline)
{
    std::lock_guard<std::mutex> mutex(this->accountLock);
    if(this->bestDeadline.find(accountId) != this->bestDeadline.end())
    {
        if(deadline < this->bestDeadline[accountId])
        {
            this->bestDeadline[accountId] = deadline;
            this->bestNonce[accountId] = nonce;
            MinerLogger::write("best deadline "+std::to_string(deadline)+" seconds for account "+std::to_string(accountId)+" using nonce "+std::to_string(nonce));
        }
    }
    else
    {
        this->bestDeadline.insert(std::make_pair(accountId, deadline));
        this->bestNonce.insert(std::make_pair(accountId, nonce));
    }
}

void Burst::Miner::nonceSubmitReport(uint64_t nonce, uint64_t accountId, uint64_t deadline)
{
    std::lock_guard<std::mutex> mutex(this->accountLock);
    if(this->bestDeadlineConfirmed.find(accountId) != this->bestDeadlineConfirmed.end())
    {
        if(deadline < this->bestDeadlineConfirmed[accountId])
        {
            this->bestDeadlineConfirmed[accountId] = deadline;
        }
    }
    else
    {
        this->bestDeadlineConfirmed.insert(std::make_pair(accountId, deadline));
    }
}