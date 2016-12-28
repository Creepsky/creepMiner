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
#include <Poco/Observer.h>

Burst::PlotReader::PlotReader(Miner& miner, const std::string& path)
	: Task(path), miner_{miner}
{
	inputPath_ = path;
	accountId_ = stoull(getAccountIdFromPlotFile(path));
	nonceStart_ = stoull(getStartNonceFromPlotFile(path));
	nonceCount_ = stoull(getNonceCountFromPlotFile(path));
	staggerSize_ = stoull(getStaggerSizeFromPlotFile(path));
	scoopNum_ = miner_.getScoopNum();
	gensig_ = miner_.getGensig();
	//readBuffer_ = &buffer_[0];
	//writeBuffer_ = &buffer_[1];
}

void Burst::PlotReader::runTask()
{
	std::ifstream inputStream(inputPath_, std::ifstream::binary);

	if (inputStream.good())
	{
		//runVerify_ = true;
		//std::thread verifierThreadObj(&PlotReader::verifierThread, this);

		std::vector<ScoopData> buffer;
		size_t chunkNum = 0;
		auto totalChunk = static_cast<size_t>(std::ceil(static_cast<double>(nonceCount_) / static_cast<double>(staggerSize_)));
		//verifySignaled_ = false;

		//MinerLogger::write("reading plot file " + inputPath);

		while (!isCancelled() && inputStream.good() && chunkNum <= totalChunk)
		{
			auto scoopBufferSize = MinerConfig::getConfig().maxBufferSizeMB * 1024 * 1024 / (64 * 2); // 8192
			auto scoopBufferCount = static_cast<size_t>(
				std::ceil(static_cast<float>(this->staggerSize_ * Settings::ScoopSize) / static_cast<float>(scoopBufferSize)));
			auto startByte = scoopNum_ * Settings::ScoopSize * staggerSize_ + chunkNum * staggerSize_ * Settings::PlotSize;
			auto scoopDoneRead = 0u;
			size_t staggerOffset;

			while (!isCancelled() && inputStream.good() && scoopDoneRead <= scoopBufferCount)
			{
				buffer.resize(scoopBufferSize / Settings::ScoopSize);
				staggerOffset = scoopDoneRead * scoopBufferSize;

				if (scoopBufferSize > (staggerSize_ * Settings::ScoopSize - (scoopDoneRead * scoopBufferSize)))
				{
					scoopBufferSize = staggerSize_ * Settings::ScoopSize - (scoopDoneRead * scoopBufferSize);
					if (scoopBufferSize > Settings::ScoopSize)
					{
						buffer.resize(scoopBufferSize / Settings::ScoopSize);
					}
				}

				if (scoopBufferSize > Settings::ScoopSize)
				{
					inputStream.seekg(startByte + staggerOffset);
					auto scoopData = reinterpret_cast<char*>(&buffer[0]);
					inputStream.read(scoopData, scoopBufferSize);

					//std::unique_lock<std::mutex> verifyLock(verifyMutex_);

					//MinerLogger::write("chunk "+std::to_string(chunkNum)+" offset "+std::to_string(startByte + staggerOffset)+" read "+std::to_string(scoopBufferSize)+" nonce offset "+std::to_string(this->nonceOffset)+" nonceRead "+std::to_string(this->nonceRead));
					
					/*std::swap(readBuffer_, writeBuffer_);
					verifySignaled_ = true;
					verifySignal_.notify_one();
					verifyLock.unlock();*/

					auto nonceRead = nonceRead_;

					for (size_t i = 0; i < buffer.size() && !isCancelled(); i++)
					{
						HashData target;
						auto test = buffer.data() + i;
						hash.update(gensig_.data(), Settings::HashSize);
						hash.update(test, Settings::ScoopSize);
						hash.close(&target[0]);

						uint64_t targetResult = 0;
						memcpy(&targetResult, &target[0], sizeof(decltype(targetResult)));
						auto deadline = targetResult / miner_.getBaseTarget();

						auto nonceNum = nonceStart_ + nonceRead + i;
						miner_.submitNonce(nonceNum, accountId_, deadline, inputPath_);
						++nonceRead_;
					}

					/*while (verifySignaled_)
					{
						std::this_thread::sleep_for(std::chrono::milliseconds(1));
						verifySignal_.notify_one();
					};*/

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

		/*std::unique_lock<std::mutex> verifyLock(verifyMutex_);
		runVerify_ = false;
		readBuffer_->clear();
		writeBuffer_->clear();
		verifySignaled_ = true;
		verifyLock.unlock();
		verifySignal_.notify_all();

		verifierThreadObj.join();*/

		//MinerLogger::write("plot read done. "+Burst::getFileNameFromPath(this->inputPath)+" = "+std::to_string(this->nonceRead)+" nonces ");
	}
}
/*
void Burst::PlotReader::verifierThread()
{
	std::unique_lock<std::mutex> verifyLock(this->verifyMutex_);

	while (runVerify_ && !isCancelled())
	{
		do
		{
			verifySignal_.wait(verifyLock);
		}
		while (!verifySignaled_ && runVerify_ && !isCancelled());

		auto nonceReadCopy = nonceRead_;
		verifySignaled_ = false;

		for (size_t i = 0; i < readBuffer_->size() && runVerify_ && !isCancelled(); i++)
		{
			HashData target;
			auto test = readBuffer_->data() + i;
			hash.update(gensig_.data(), Settings::HashSize);
			hash.update(test, Settings::ScoopSize);
			hash.close(&target[0]);

			uint64_t targetResult = 0;
			memcpy(&targetResult, &target[0], sizeof(decltype(targetResult)));
			auto deadline = targetResult / miner_.getBaseTarget();

			auto nonceNum = nonceStart_ + nonceReadCopy + i;
			miner_.submitNonce(nonceNum, accountId_, deadline, inputPath_);
			++nonceRead_;
		}

		//MinerLogger::write("verifier processed "+std::to_string(this->nonceRead)+" readsize "+std::to_string(this->readBuffer->size()));
	}
	//MinerLogger::write("plot read done. "+std::to_string(this->nonceRead)+" nonces ");

	if (MinerConfig::getConfig().output.plotDone)
		MinerLogger::write("Plot " + inputPath_ + " done", TextType::Unimportant);
}*/

Burst::PlotListReader::PlotListReader(Miner& miner, std::shared_ptr<PlotReadProgress> progress,
	std::string&& dir, std::vector<std::shared_ptr<PlotFile>>&& plotFiles)
	: Task(dir),
	plotFileList_{std::move(plotFiles)}, miner_(&miner), progress_(progress), dir_{std::move(dir)}
{}

void Burst::PlotListReader::runTask()
{
	if (miner_ == nullptr)
		return;

	auto iter = plotFileList_.begin();

	while (iter != plotFileList_.end() && !isCancelled())
	{
		auto path = (*iter)->getPath();
		PlotReader plotReader{*miner_, path};
		Poco::ThreadPool::defaultPool().start(plotReader);

		// we have to react to the stop-flag
		while (plotReader.state() != TASK_FINISHED)
		{
			if (isCancelled())
			{
				MinerLogger::write("stopping single plot reader", TextType::Debug);
				plotReader.cancel();
				// wait for cancellation
				while (plotReader.state() != TASK_FINISHED)
					std::this_thread::sleep_for(std::chrono::milliseconds{10});
				MinerLogger::write("stopped single plot reader", TextType::Debug);
			}
			else
			{
				std::this_thread::sleep_for(std::chrono::milliseconds{100});
			}
		}

		//MinerLogger::write("waiting for plot-reader to finish...", TextType::Debug);
		//MinerLogger::write("plot-reader finished", TextType::Debug);

		if (progress_ != nullptr &&
			MinerConfig::getConfig().output.progress &&
			!isCancelled())
		{
			progress_->add((*iter)->getSize());
		}

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
