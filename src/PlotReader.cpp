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
#include <Poco/NotificationQueue.h>
#include "PlotVerifier.hpp"
#include <Poco/Timestamp.h>
#include "Output.hpp"

Burst::GlobalBufferSize Burst::PlotReader::globalBufferSize;

void Burst::GlobalBufferSize::reset(uint64_t max)
{
	Poco::FastMutex::ScopedLock lock{ mutex_ };
	size_ = 0;
	max_ = max;
}

bool Burst::GlobalBufferSize::add(uint64_t sizeToAdd)
{
	Poco::FastMutex::ScopedLock lock{ mutex_ };

	if (size_ + sizeToAdd > max_)
		return false;

	size_ += sizeToAdd;
	return true;
}

void Burst::GlobalBufferSize::remove(uint64_t sizeToRemove)
{
	Poco::FastMutex::ScopedLock lock{ mutex_ };

	if (sizeToRemove > size_)
		sizeToRemove = size_;

	size_ -= sizeToRemove;
}

Burst::PlotReader::PlotReader(Miner& miner, std::shared_ptr<Burst::PlotReadProgress> progress,
	Poco::NotificationQueue& verificationQueue, Poco::NotificationQueue& plotReadQueue)
	: Task("PlotReader"), miner_{miner}, progress_{progress}, verificationQueue_{&verificationQueue}, plotReadQueue_(&plotReadQueue)
{
	//scoopNum_ = miner_.getScoopNum();
	//gensig_ = miner_.getGensig();
	//plotList_ = &plotList;
}

void Burst::PlotReader::runTask()
{
	while (!isCancelled())
	{
		Poco::Notification::Ptr notification(plotReadQueue_->waitDequeueNotification());
		PlotReadNotification::Ptr plotReadNotification;

		if (notification)
			plotReadNotification = notification.cast<PlotReadNotification>();
		else
			break;

		Poco::Timestamp timeStartDir;
		auto block = miner_.getBlockheight();
		auto gensig = miner_.getGensig();

		for (auto plotFileIter = plotReadNotification->plotList.begin();
			plotFileIter != plotReadNotification->plotList.end() &&
			!isCancelled() &&
			miner_.getBlockheight() == plotReadNotification->blockheight;
			++plotFileIter)
		{
			auto& plotFile = *(*plotFileIter);
			std::ifstream inputStream(plotFile.getPath(), std::ifstream::in | std::ifstream::binary);
			
			Poco::Timestamp timeStartFile;
			
			if (inputStream.is_open())
			{
				auto accountId = stoull(getAccountIdFromPlotFile(plotFile.getPath()));
				auto nonceStart = stoull(getStartNonceFromPlotFile(plotFile.getPath()));
				auto nonceCount = stoull(getNonceCountFromPlotFile(plotFile.getPath()));
				auto staggerSize = stoull(getStaggerSizeFromPlotFile(plotFile.getPath()));

				//MinerLogger::write("reading plot file " + plotFile.getPath());

				const auto staggerBlocks = nonceCount / staggerSize;
				const auto staggerBlockSize = staggerSize * Settings::PlotSize;
				const auto staggerScoopSize = staggerSize * Settings::ScoopSize;
				
				for (auto staggerBlock = 0ul; staggerBlock < staggerBlocks; ++staggerBlock)
				{
					while (!isCancelled() &&
						miner_.getBlockheight() == plotReadNotification->blockheight &&
						!globalBufferSize.add(staggerScoopSize))
					{ }

					if (isCancelled() || miner_.getBlockheight() != plotReadNotification->blockheight)
						break;

					VerifyNotification::Ptr verification = new VerifyNotification{};
					verification->accountId = accountId;
					verification->nonceStart = nonceStart;
					verification->block = block;
					verification->inputPath = plotFile.getPath();
					verification->gensig = gensig;
					verification->buffer.resize(staggerSize);

					auto staggerBlockOffset = staggerBlock * staggerBlockSize;
					auto staggerScoopOffset = plotReadNotification->scoopNum * staggerSize * Settings::ScoopSize;

					inputStream.seekg(staggerBlockOffset + staggerScoopOffset);
					
					inputStream.read(reinterpret_cast<char*>(&verification->buffer[0]), staggerScoopSize);
					
					verification->nonceRead = staggerBlock * staggerSize;
										
					verificationQueue_->enqueueNotification(verification);
				}

				inputStream.close();

				if (progress_ != nullptr && !isCancelled())
				{
					progress_->add(plotFile.getSize());
					miner_.getData().getBlockData()->setProgress(progress_->getProgress());
				}

				//MinerLogger::write("finished reading plot file " + inputPath_);
				//MinerLogger::write("plot read done. "+Burst::getFileNameFromPath(this->inputPath)+" = "+std::to_string(this->nonceRead)+" nonces ");
			}

			//if (MinerConfig::getConfig().output.debug)
				//MinerLogger::write("finished reading plot file " + plotFile.getPath(), TextType::Debug);

			auto fileReadDiff = timeStartFile.elapsed();
			auto fileReadDiffSeconds = static_cast<float>(fileReadDiff) / 1000 / 1000;
			Poco::Timespan span{fileReadDiff};

			log_information_if(MinerLogger::plotReader, MinerLogger::hasOutput(PlotDone), "%s done %s in %ss (~%.2hf GB/s)",
				plotFile.getPath(),
				memToString(plotFile.getSize(), 2),
				Poco::DateTimeFormatter::format(span, "%s.%i"),
				static_cast<float>(plotFile.getSize()) / 1024 / 1024 / 1024 / fileReadDiffSeconds);
		}
		
		auto dirReadDiff = timeStartDir.elapsed();
		auto dirReadDiffSeconds = static_cast<float>(dirReadDiff) / 1000 / 1000;
		Poco::Timespan span{dirReadDiff};

		std::uint64_t totalSizeBytes = 0u;

		for (const auto& plot : plotReadNotification->plotList)
			totalSizeBytes += plot->getSize();

		auto totalSizeMBytes = totalSizeBytes / 1024 / 1024 / 1024;

		log_information_if(MinerLogger::plotReader, MinerLogger::hasOutput(DirDone), "Dir %s done: %z files, %s total, %ss (~%.2hf GB/s)",
			plotReadNotification->dir,
			plotReadNotification->plotList.size(),
			memToString(totalSizeBytes, 2),
			Poco::DateTimeFormatter::format(span, "%s.%i"),
			totalSizeMBytes / dirReadDiffSeconds);
	}
}

void Burst::PlotReadProgress::reset()
{
	Poco::Mutex::ScopedLock guard(lock_);
	progress_ = 0;
}

void Burst::PlotReadProgress::add(uintmax_t value)
{
	Poco::Mutex::ScopedLock guard(lock_);
	progress_ += value;

	if (max_ > 0)
		MinerLogger::writeProgress(getProgress(), 48);
}

void Burst::PlotReadProgress::set(uintmax_t value)
{
	Poco::Mutex::ScopedLock guard(lock_);
	progress_ = value;
}

void Burst::PlotReadProgress::setMax(uintmax_t value)
{
	Poco::Mutex::ScopedLock guard(lock_);
	max_ = value;
}

bool Burst::PlotReadProgress::isReady() const
{
	Poco::Mutex::ScopedLock guard(lock_);
	return progress_ >= max_;
}

uintmax_t Burst::PlotReadProgress::getValue() const
{
	Poco::Mutex::ScopedLock guard(lock_);
	return progress_;
}

float Burst::PlotReadProgress::getProgress() const
{
	Poco::Mutex::ScopedLock guard(lock_);
	return progress_ * 1.f / max_ * 100;
}
