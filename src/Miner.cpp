//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#include "Miner.h"
#include "MinerLogger.h"
#include "MinerConfig.h"
#include "PlotReader.h"
#include "MinerUtil.h"
#include "nxt/nxt_address.h"
#include <random>
#include <deque>

Burst::Miner::Miner(MinerConfig& config)
	: scoopNum(0), baseTarget(0), blockHeight(0)
{
	this->config = &config;
	this->running = false;
}

Burst::Miner::~Miner()
{}

void Burst::Miner::run()
{
    this->running = true;
    std::thread submitter(&Miner::nonceSubmitterThread, this);
    
	progress = std::make_shared<PlotReadProgress>();

	if(!this->protocol.run(this))
        MinerLogger::write("Mining networking failed", TextType::Error);

    this->running = false;
    submitter.join();
}

void Burst::Miner::stop()
{
	this->running = false;
}

const Burst::MinerConfig* Burst::Miner::getConfig() const
{
    return this->config;
}

void Burst::Miner::updateGensig(const std::string gensigStr, uint64_t blockHeight, uint64_t baseTarget)
{
	std::lock_guard<std::mutex> lock(deadlinesLock);

	// stop all reading processes if any
	for (auto& plotReader : plotReaders)
		plotReader->stop();

	// wait for all plotReaders to stop
	auto stopped = false;
	//
	while (!stopped)
	{
		stopped = true;

		for (auto& plotReader : plotReaders)
		{
			if (!plotReader->isDone())
			{
				stopped = false;
				break;
			}
		}
	}

    this->gensigStr = gensigStr;
    this->blockHeight = blockHeight;
    this->baseTarget = baseTarget;
	this->deadlines.clear();
    
	for (auto i = 0; i < 32; ++i)
    {
	    auto byteStr = gensigStr.substr(i*2,2);
        this->gensig[i] = static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16));
    }
    
    GensigData newGenSig;
    this->hash.update(&this->gensig[0],this->gensig.size());
    this->hash.update(blockHeight);
	this->hash.close(&newGenSig[0]);
	this->scoopNum = (static_cast<int>(newGenSig[newGenSig.size()-2] & 0x0F) << 8) | static_cast<int>(newGenSig[newGenSig.size()-1]);

	MinerLogger::write(std::string(76, '-'), TextType::Information);
	MinerLogger::write("block#      " + std::to_string(blockHeight), TextType::Information);
	MinerLogger::write("scoop#      " + std::to_string(scoopNum), TextType::Information);
	MinerLogger::write("baseTarget# " + std::to_string(baseTarget), TextType::Information);
	MinerLogger::write("gensig#     " + gensigStr, TextType::Information);
	MinerLogger::write(std::string(76, '-'), TextType::Information);

	// this block is closed in itself
	// dont use the variables in it outside!
	{
		using PlotList = std::vector<std::shared_ptr<PlotFile>>;
		std::unordered_map<std::string, PlotList> plotDirs;

		for(const auto plotFile : this->config->getPlotFiles())
		{
			auto last_slash_idx = plotFile->getPath().find_last_of("/\\");

			std::string dir;

			if (last_slash_idx == std::string::npos)
				continue;

			dir = plotFile->getPath().substr(0, last_slash_idx);
		
			auto iter = plotDirs.find(dir);

			if (iter == plotDirs.end())
				plotDirs.emplace(std::make_pair(dir, PlotList{}));

			plotDirs[dir].emplace_back(plotFile);
		}
		
		progress->reset();
		progress->setMax(config->getTotalPlotsize());

		for (auto& plotDir : plotDirs)
		{
			auto reader = std::make_shared<PlotListReader>(*this, progress);
			reader->read(std::move(plotDir.second));
			plotReaders.emplace_back(reader);
		}
	}
    
    std::random_device rd;
    std::default_random_engine randomizer(rd());
    //std::uniform_int_distribution<size_t> dist(0, this->config->submissionMaxDelay);
	std::uniform_int_distribution<size_t> dist(0, 3);
	auto delay = dist(randomizer);

    this->nextNonceSubmission = std::chrono::system_clock::now() +
                                std::chrono::seconds(delay);
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
    std::unique_lock<std::mutex> lock(this->deadlinesLock, std::defer_lock);

    while(this->running)
    {		
        if(std::chrono::system_clock::now() > this->nextNonceSubmission)
		  //|| std::all_of(plotReaders.begin(), plotReaders.end(), [](std::shared_ptr<PlotListReader> reader) { return reader->isDone(); }))
        {
			lock.lock();

            for (auto accountDeadlines : deadlines)
            {
				auto deadline = accountDeadlines.second.getBestDeadline();
				auto bestConfirmed = accountDeadlines.second.getBestConfirmed();

				if (deadline == nullptr)
					continue;

                if(deadline != bestConfirmed)
                {
					std::thread sendThread([this, deadline]()
					{
						static std::set<std::shared_ptr<Deadline>> deadlinesToSend;
						static std::mutex deadlinesToSendMutex;

						std::unique_lock<std::mutex> _lock(deadlinesToSendMutex);

						if (deadlinesToSend.find(deadline) != deadlinesToSend.end())
						{
							_lock.unlock();
							return;
						}

						deadlinesToSend.emplace(deadline);

						_lock.unlock();

						auto nonce = deadline->getNonce();
						auto deadlineValue = deadline->getDeadline();
						auto accountId = deadline->getAccountId();

						//MinerLogger::write("sending nonce from thread, " + deadlineFormat(deadlineValue), TextType::System);

						if(protocol.submitNonce(nonce, accountId, deadlineValue) == SubmitResponse::Submitted)
						{
							this->nonceSubmitReport(nonce, accountId, deadlineValue);

							_lock.lock();
							deadlinesToSend.erase(deadline);
							_lock.unlock();
						}
						else
						{
							MinerLogger::write("nonce not submitted!", TextType::Error);
						}

						//MinerLogger::write("leaving thread with deadline " + deadlineFormat(deadlineValue), TextType::System);
					});

					sendThread.detach();
                }
            }

			lock.unlock();

            //std::random_device rd;
            //std::default_random_engine randomizer(rd());
            //std::uniform_int_distribution<size_t> dist(0, this->config->submissionMaxDelay);
            //this->nextNonceSubmission = std::chrono::system_clock::now() + std::chrono::seconds(dist(randomizer));
			this->nextNonceSubmission = std::chrono::system_clock::now() + std::chrono::seconds(2);
        }
		
		std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void Burst::Miner::submitNonce(uint64_t nonce, uint64_t accountId, uint64_t deadline)
{
    std::lock_guard<std::mutex> mutex(deadlinesLock);

	auto bestDeadline = deadlines[accountId].getBestDeadline();

	// is the new nonce better then the best one we already have?
	if (bestDeadline == nullptr || bestDeadline->getDeadline() > deadline)
	{
		deadlines[accountId].add({ nonce, deadline, accountId, blockHeight });
		MinerLogger::write(NxtAddress(accountId).to_string() + ": deadline found (" + Burst::deadlineFormat(deadline) + ")", TextType::Ok);
	}
}

void Burst::Miner::nonceSubmitReport(uint64_t nonce, uint64_t accountId, uint64_t deadline)
{
    std::lock_guard<std::mutex> mutex(deadlinesLock);
    
	if (deadlines[accountId].confirm(nonce, accountId, blockHeight))
		if (deadlines[accountId].getBestConfirmed()->getDeadline() == deadline)
			MinerLogger::write(NxtAddress(accountId).to_string() + ": deadline confirmed (" + deadlineFormat(deadline) + ")", TextType::Success);
}
