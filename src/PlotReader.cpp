//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#include "PlotReader.hpp"
#include "MinerUtil.hpp"
#include "MinerLogger.hpp"
#include "MinerConfig.hpp"
#include <fstream>
#include "Miner.hpp"
#include <cmath>

Burst::PlotReader::PlotReader(Miner& miner)
	: Burst::PlotReader()
{
	done_ = true;
	miner_ = &miner;
}

Burst::PlotReader::~PlotReader()
{
	this->stop();
}

void Burst::PlotReader::stop()
{
	stopped_ = true;

	if (readerThreadObj_.joinable())
		readerThreadObj_.join();
}

bool Burst::PlotReader::isDone() const
{
	return done_;
}

void Burst::PlotReader::read(const std::string& path)
{
	stop();
	accountId_ = stoull(getAccountIdFromPlotFile(path));
	nonceStart_ = stoull(getStartNonceFromPlotFile(path));
	nonceCount_ = stoull(getNonceCountFromPlotFile(path));
	staggerSize_ = stoull(getStaggerSizeFromPlotFile(path));
	scoopNum_ = this->miner_->getScoopNum();
	gensig_ = miner_->getGensig();
	done_ = false;
	stopped_ = false;
	inputPath_ = path;
	readerThread();
}

void Burst::PlotReader::readerThread()
{
	std::ifstream inputStream(inputPath_, std::ifstream::binary);

	if (inputStream.good())
	{
		runVerify_ = true;
		std::thread verifierThreadObj(&PlotReader::verifierThread, this);

		size_t chunkNum = 0;
		auto totalChunk = static_cast<size_t>(std::ceil(static_cast<double>(this->nonceCount_) / static_cast<double>(this->staggerSize_)));
		nonceOffset_ = 0;
		nonceRead_ = 0;
		verifySignaled_ = false;

		readBuffer_ = &buffer_[0];
		writeBuffer_ = &buffer_[1];

		//MinerLogger::write("reading plot file " + inputPath);

		while (!this->done_ && !stopped_ && inputStream.good() && chunkNum <= totalChunk)
		{
			auto scoopBufferSize = MinerConfig::getConfig().maxBufferSizeMB * 1024 * 1024 / (64 * 2); // 8192
			auto scoopBufferCount = static_cast<size_t>(
				std::ceil(static_cast<float>(this->staggerSize_ * Settings::ScoopSize) / static_cast<float>(scoopBufferSize)));
			auto startByte = scoopNum_ * Settings::ScoopSize * staggerSize_ + chunkNum * staggerSize_ * Settings::PlotSize;
			auto scoopDoneRead = 0u;
			size_t staggerOffset;

			while (!done_ && !stopped_ && inputStream.good() && scoopDoneRead <= scoopBufferCount)
			{
				writeBuffer_->resize(scoopBufferSize / Settings::ScoopSize);
				staggerOffset = scoopDoneRead * scoopBufferSize;

				if (scoopBufferSize > (staggerSize_ * Settings::ScoopSize - (scoopDoneRead * scoopBufferSize)))
				{
					scoopBufferSize = staggerSize_ * Settings::ScoopSize - (scoopDoneRead * scoopBufferSize);
					if (scoopBufferSize > Settings::ScoopSize)
					{
						writeBuffer_->resize(scoopBufferSize / Settings::ScoopSize);
					}
				}

				if (scoopBufferSize > Settings::ScoopSize)
				{
					inputStream.seekg(startByte + staggerOffset);
					auto scoopData = reinterpret_cast<char*>(&(*writeBuffer_)[0]);
					inputStream.read(scoopData, scoopBufferSize);

					std::unique_lock<std::mutex> verifyLock(verifyMutex_);

					//MinerLogger::write("chunk "+std::to_string(chunkNum)+" offset "+std::to_string(startByte + staggerOffset)+" read "+std::to_string(scoopBufferSize)+" nonce offset "+std::to_string(this->nonceOffset)+" nonceRead "+std::to_string(this->nonceRead));
					
					std::swap(readBuffer_, writeBuffer_);
					verifySignaled_ = true;
					verifySignal_.notify_one();
					verifyLock.unlock();

					while (verifySignaled_)
					{
						std::this_thread::sleep_for(std::chrono::milliseconds(1));
						verifySignal_.notify_one();
					};

					nonceOffset_ = chunkNum * this->staggerSize_ + scoopDoneRead * (scoopBufferSize / Settings::ScoopSize);
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

		std::unique_lock<std::mutex> verifyLock(verifyMutex_);
		runVerify_ = false;
		readBuffer_->clear();
		writeBuffer_->clear();
		verifySignaled_ = true;
		verifyLock.unlock();
		verifySignal_.notify_all();

		verifierThreadObj.join();

		done_ = true;
		stopped_ = true;

		//MinerLogger::write("plot read done. "+Burst::getFileNameFromPath(this->inputPath)+" = "+std::to_string(this->nonceRead)+" nonces ");
	}
}

void Burst::PlotReader::verifierThread()
{
	std::unique_lock<std::mutex> verifyLock(this->verifyMutex_);

	while (runVerify_)
	{
		do
		{
			verifySignal_.wait(verifyLock);
		}
		while (!verifySignaled_ && runVerify_);

		auto nonceReadCopy = nonceRead_;
		verifySignaled_ = false;

		for (size_t i = 0; i < readBuffer_->size() && runVerify_; i++)
		{
			HashData target;
			auto test = readBuffer_->data() + i;
			hash.update(gensig_.data(), Settings::HashSize);
			hash.update(test, Settings::ScoopSize);
			hash.close(&target[0]);

			uint64_t targetResult = 0;
			memcpy(&targetResult, &target[0], sizeof(decltype(targetResult)));
			auto deadline = targetResult / miner_->getBaseTarget();

			auto nonceNum = nonceStart_ + nonceReadCopy + i;
			miner_->submitNonce(nonceNum, accountId_, deadline, inputPath_);
			++nonceRead_;
		}

		//MinerLogger::write("verifier processed "+std::to_string(this->nonceRead)+" readsize "+std::to_string(this->readBuffer->size()));
	}
	//MinerLogger::write("plot read done. "+std::to_string(this->nonceRead)+" nonces ");

	if (MinerConfig::getConfig().output.plotDone)
		MinerLogger::write("Plot " + inputPath_ + " done", TextType::Unimportant);
}

Burst::PlotListReader::PlotListReader(Miner& miner, std::shared_ptr<PlotReadProgress> progress,
	std::string&& dir, std::vector<std::shared_ptr<PlotFile>>&& plotFiles)
	: Task(""),
	miner_(&miner), progress_(progress), dir_{std::move(dir)}, plotFileList_{std::move(plotFiles)}
{}

void Burst::PlotListReader::runTask()
{
	if (miner_ == nullptr)
		return;

	auto iter = plotFileList_.begin();

	while (iter != plotFileList_.end() && !isCancelled())
	{
		auto path = (*iter)->getPath();
		PlotReader plotReader { *miner_ };

		// we create a new thread, which will run the plot-reader
		std::thread readThread([&plotReader, path]() { plotReader.read(path); });

		// we have to react to the stop-flag
		while (!plotReader.isDone())
		{
			if (isCancelled())
			{
				MinerLogger::write("stopping single plot reader", TextType::Debug);
				plotReader.stop();
			}
		}

		//MinerLogger::write("waiting for plot-reader to finish...", TextType::Debug);
		readThread.join();
		//MinerLogger::write("plot-reader finished", TextType::Debug);

		if (progress_ != nullptr && MinerConfig::getConfig().output.progress)
			progress_->add((*iter)->getSize());

		++iter;
	}

	MinerLogger::write("plot list reader finished reading plot-dir " + dir_, TextType::Debug);

	if (MinerConfig::getConfig().output.dirDone)
		MinerLogger::write("Dir " + dir_ + " done", TextType::Unimportant);
}

void Burst::PlotReadProgress::reset()
{
	std::lock_guard<std::mutex> guard(lock_);
	progress_ = 0;
}

void Burst::PlotReadProgress::add(uintmax_t value)
{
	std::lock_guard<std::mutex> guard(lock_);
	progress_ += value;

	if (max_ > 0 && MinerConfig::getConfig().output.progress)
		MinerLogger::writeProgress(progress_ * 1.f / max_ * 100, 48);
		//MinerLogger::write("progress: " + std::to_string(progress * 1.f / max * 100) + " %",
						   //TextType::Progress);
}

void Burst::PlotReadProgress::set(uintmax_t value)
{
	std::lock_guard<std::mutex> guard(lock_);
	progress_ = value;
}

void Burst::PlotReadProgress::setMax(uintmax_t value)
{
	std::lock_guard<std::mutex> guard(lock_);
	max_ = value;
}

bool Burst::PlotReadProgress::isReady() const
{
	return progress_ >= max_;
}
