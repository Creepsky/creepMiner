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
    
	MinerLogger::write("block#" + std::to_string(blockHeight) + " s#:" + std::to_string(this->scoopNum)+" bt:"+std::to_string(this->baseTarget)+" "+this->gensigStr);
    
    this->bestDeadline.clear();
    this->config->rescan();
    
    this->plotReaders.clear();
    for(const std::string path : this->config->plotList)
    {
		std::shared_ptr<PlotReader> reader = std::shared_ptr<PlotReader>(new PlotReader(this));
		reader->read(path);
		this->plotReaders.push_back(reader);
    }
    
    std::random_device rd;
    std::default_random_engine randomizer(rd());
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
				if (!firstSubmit)
				{
					firstSubmit = deadline < this->bestDeadlineConfirmed[accountId];
				}
                if( firstSubmit )
                {
					bool exist = false;
					for (size_t i = 0; i < submitList.size(); i++)
					{
						if (submitList.at(i).first == nonce &&
							submitList.at(i).second == accountId)
						{
							exist = true;
							break;
						}
					}
					if (!exist)
					{
						submitList.push_back(std::make_pair(nonce, accountId));
					}
                }
            }
            mutex.unlock();
            
            std::random_device rd;
            std::default_random_engine randomizer(rd());
            std::uniform_int_distribution<size_t> dist(0, this->config->submissionMaxDelay);
            this->nextNonceSubmission = std::chrono::system_clock::now() +
            std::chrono::seconds(dist(randomizer));
            
            if(submitList.size() > 0)
            {
				mutex.lock();
				
					auto submitData = submitList.front();
					submitList.pop_front();
				
				mutex.unlock();
                
				uint64_t confirmedDeadline = this->protocol.submitNonce(submitData.first, submitData.second);
                
                if(confirmedDeadline != (uint64_t)(-1))
                {
					this->nonceSubmitReport(submitData.first, submitData.second, confirmedDeadline);
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void Burst::Miner::submitNonce(uint64_t nonce, uint64_t accountId, uint64_t deadline)
{
    std::lock_guard<std::mutex> mutex(this->accountLock);
	NxtAddress addr(accountId);
    if(this->bestDeadline.find(accountId) != this->bestDeadline.end())
    {
        if(deadline < this->bestDeadline[accountId])
        {
            this->bestDeadline[accountId] = deadline;
            this->bestNonce[accountId] = nonce;
            
            MinerLogger::write(addr.to_string()+" dl:"+Burst::deadlineFormat(deadline)+" n:"+std::to_string(nonce));
        }
    }
    else
    {
        this->bestDeadline.insert(std::make_pair(accountId, deadline));
        this->bestNonce.insert(std::make_pair(accountId, nonce));
		MinerLogger::write(addr.to_string() + " dl:" + Burst::deadlineFormat(deadline) + " n:" + std::to_string(nonce));
    }
}

void Burst::Miner::nonceSubmitReport(uint64_t nonce, uint64_t accountId, uint64_t deadline)
{
    std::lock_guard<std::mutex> mutex(this->accountLock);
	NxtAddress addr(accountId);
    if(this->bestDeadlineConfirmed.find(accountId) != this->bestDeadlineConfirmed.end())
    {
        if(deadline < this->bestDeadlineConfirmed[accountId])
        {
            this->bestDeadlineConfirmed[accountId] = deadline;
			MinerLogger::write("confirmed dl. for " + addr.to_string() + " : " + Burst::deadlineFormat(deadline));
        }
    }
    else
    {
        this->bestDeadlineConfirmed.insert(std::make_pair(accountId, deadline));
		MinerLogger::write("confirmed dl. for " + addr.to_string() + " : " + Burst::deadlineFormat(deadline));
    }
}