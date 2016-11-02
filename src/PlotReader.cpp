//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#include "PlotReader.h"
#include "MinerUtil.h"
#include "MinerLogger.h"
#include "MinerConfig.h"
#include <fstream>
#include "Miner.h"
#include <cmath>
#include <cstring>

Burst::PlotReader::PlotReader(Miner& miner)
	: Burst::PlotReader()
{
	this->done = true;
	this->miner = &miner;
}

Burst::PlotReader::~PlotReader()
{
	this->stop();
}

void Burst::PlotReader::stop()
{
	this->done = true;

	if (readerThreadObj.joinable())
		readerThreadObj.join();
}

bool Burst::PlotReader::isDone() const
{
	return done;
}

void Burst::PlotReader::read(const std::string& path)
{
	stop();
	accountId = stoull(getAccountIdFromPlotFile(path));
	nonceStart = stoull(getStartNonceFromPlotFile(path));
	nonceCount = stoull(getNonceCountFromPlotFile(path));
	staggerSize = stoull(getStaggerSizeFromPlotFile(path));
	scoopNum = this->miner->getScoopNum();
	gensig = miner->getGensig();
	done = false;
	inputPath = path;
	//readerThreadObj = std::thread(&PlotReader::readerThread, this);
	readerThread();
}

void Burst::PlotReader::readerThread()
{
	std::ifstream inputStream(this->inputPath, std::ifstream::binary);

	if (inputStream.good())
	{
		this->runVerify = true;
		std::thread verifierThreadObj(&PlotReader::verifierThread, this);

		size_t chunkNum = 0;
		auto totalChunk = static_cast<size_t>(std::ceil(static_cast<double>(this->nonceCount) / static_cast<double>(this->staggerSize)));
		this->nonceOffset = 0;
		this->nonceRead = 0;
		this->verifySignaled = false;

		this->readBuffer = &this->buffer[0];
		this->writeBuffer = &this->buffer[1];

		//MinerLogger::write("reading plot file " + inputPath);

		while (!this->done && inputStream.good() && chunkNum <= totalChunk)
		{
			auto scoopBufferSize = MinerConfig::getConfig().maxBufferSizeMB * 1024 * 1024 / (64 * 2);
			auto scoopBufferCount = static_cast<size_t>(
				std::ceil(static_cast<float>(this->staggerSize * Settings::ScoopSize) / static_cast<float>(scoopBufferSize)));
			auto startByte = this->scoopNum * Settings::ScoopSize * this->staggerSize + chunkNum * this->staggerSize * Settings::PlotSize;
			auto scoopDoneRead = 0;
			size_t staggerOffset;

			while (!this->done && inputStream.good() && scoopDoneRead <= scoopBufferCount)
			{
				this->writeBuffer->resize(scoopBufferSize / Settings::ScoopSize);
				staggerOffset = scoopDoneRead * scoopBufferSize;

				if (scoopBufferSize > (this->staggerSize * Settings::ScoopSize - (scoopDoneRead * scoopBufferSize)))
				{
					scoopBufferSize = this->staggerSize * Settings::ScoopSize - (scoopDoneRead * scoopBufferSize);
					if (scoopBufferSize > Settings::ScoopSize)
					{
						this->writeBuffer->resize(scoopBufferSize / Settings::ScoopSize);
					}
				}

				if (scoopBufferSize > Settings::ScoopSize)
				{
					inputStream.seekg(startByte + staggerOffset);
					auto scoopData = reinterpret_cast<char*>(&(*this->writeBuffer)[0]);
					inputStream.read(scoopData, scoopBufferSize);

					std::unique_lock<std::mutex> verifyLock(this->verifyMutex);

					//MinerLogger::write("chunk "+std::to_string(chunkNum)+" offset "+std::to_string(startByte + staggerOffset)+" read "+std::to_string(scoopBufferSize)+" nonce offset "+std::to_string(this->nonceOffset)+" nonceRead "+std::to_string(this->nonceRead));

					auto temp = this->readBuffer;
					readBuffer = this->writeBuffer;
					writeBuffer = temp;
					nonceOffset = chunkNum * this->staggerSize + scoopDoneRead * (scoopBufferSize / Settings::ScoopSize);
					verifySignaled = true;
					verifySignal.notify_one();
					verifyLock.unlock();

					while (this->verifySignaled)
					{
						std::this_thread::sleep_for(std::chrono::microseconds(1));
						this->verifySignal.notify_one();
					};
				}
				//else
				//{
				//	MinerLogger::write("scoop buffer ="+std::to_string(scoopBufferSize));
				//}

				++scoopDoneRead;
			}

			++chunkNum;
		}

		inputStream.close();

		//MinerLogger::write("finished reading plot file " + inputPath);

		std::unique_lock<std::mutex> verifyLock(this->verifyMutex);
		this->runVerify = false;
		this->readBuffer->clear();
		this->writeBuffer->clear();
		this->verifySignaled = true;
		verifyLock.unlock();
		this->verifySignal.notify_all();

		verifierThreadObj.join();

		this->done = true;

		//MinerLogger::write("plot read done. "+Burst::getFileNameFromPath(this->inputPath)+" = "+std::to_string(this->nonceRead)+" nonces ");
	}
}

void Burst::PlotReader::verifierThread()
{
	std::unique_lock<std::mutex> verifyLock(this->verifyMutex);

	while (this->runVerify)
	{
		do
		{
			this->verifySignal.wait(verifyLock);
		}
		while (!this->verifySignaled && runVerify);

		this->verifySignaled = false;

		for (size_t i = 0; i < this->readBuffer->size() && runVerify; i++)
		{
			HashData target;
			auto test = reinterpret_cast<char*>(&(*this->readBuffer)[i]);
			hash.update(&this->gensig[0], Settings::HashSize);
			hash.update(test, Settings::ScoopSize);
			hash.close(&target[0]);

			uint64_t targetResult = 0;
			memcpy(&targetResult, &target[0], sizeof(decltype(targetResult)));
			auto deadline = targetResult / this->miner->getBaseTarget();

			auto nonceNum = this->nonceStart + this->nonceOffset + i;
			miner->submitNonce(nonceNum, this->accountId, deadline);
			++nonceRead;
		}
		//MinerLogger::write("verifier processed "+std::to_string(this->nonceRead)+" readsize "+std::to_string(this->readBuffer->size()));
	}
	//MinerLogger::write("plot read done. "+std::to_string(this->nonceRead)+" nonces ");
}

Burst::PlotListReader::PlotListReader(Miner& miner, std::shared_ptr<PlotReadProgress> progress)
	: done(false), miner(&miner), progress(progress), stopped(false)
{}

Burst::PlotListReader::~PlotListReader()
{
	stop();
}

void Burst::PlotListReader::read(std::string&& dir, std::vector<std::shared_ptr<PlotFile>>&& plotFiles)
{
	done = false;
	stopped = false;
	this->dir = std::move(dir);
	plotFileList = std::move(plotFiles);
	readerThreadObj = std::thread(&PlotListReader::readThread, this);
}

void Burst::PlotListReader::stop()
{
	stopped = true;

	if (readerThreadObj.joinable())
		readerThreadObj.join();
}

bool Burst::PlotListReader::isDone() const
{
	return done;
}

void Burst::PlotListReader::readThread()
{
	if (miner == nullptr)
		return;

	PlotReader plotReader { *miner };
	auto iter = plotFileList.begin();

	while (iter != plotFileList.end() && !done)
	{
		auto path = (*iter)->getPath();

		// we create a new thread, which will run the plot-reader
		std::thread readThread([&plotReader, path]()
							   {
								   plotReader.read(path);
							   });

		// we have to react to the stop-flag
		while (!plotReader.isDone() && stopped)
		{
			plotReader.stop();
			MinerLogger::write("stopped single plot reader", TextType::Debug);
		}

		if (readThread.joinable())
			readThread.join();

		if (progress != nullptr && MinerConfig::getConfig().output.progress)
			progress->add((*iter)->getSize());

		++iter;
	}

	MinerLogger::write("plot list reader finished reading plot-dir " + dir, TextType::Debug);

	done = true;
}

void Burst::PlotReadProgress::reset()
{
	std::lock_guard<std::mutex> guard(lock);
	progress = 0;
}

void Burst::PlotReadProgress::add(uintmax_t value)
{
	std::lock_guard<std::mutex> guard(lock);
	progress += value;

	if (max > 0 && MinerConfig::getConfig().output.progress)
		MinerLogger::write("progress: " + std::to_string(progress * 1.f / max * 100) + " %",
						   TextType::Unimportant);
}

void Burst::PlotReadProgress::set(uintmax_t value)
{
	std::lock_guard<std::mutex> guard(lock);
	progress = value;
}

void Burst::PlotReadProgress::setMax(uintmax_t value)
{
	std::lock_guard<std::mutex> guard(lock);
	max = value;
}

bool Burst::PlotReadProgress::isReady() const
{
	return progress >= max;
}
