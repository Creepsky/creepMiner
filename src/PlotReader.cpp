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
#include <Poco/NotificationQueue.h>
#include "PlotVerifier.hpp"
#include <Poco/Timestamp.h>
#include "Output.hpp"

Burst::GlobalBufferSize Burst::PlotReader::globalBufferSize;

void Burst::GlobalBufferSize::setMax(uint64_t max)
{
	Poco::FastMutex::ScopedLock lock{ mutex_ };
	max_ = max;
}

bool Burst::GlobalBufferSize::reserve(uint64_t size)
{
	Poco::FastMutex::ScopedLock lock{ mutex_ };
	
	if (size_ + size > max_)
		return false;

	size_ += size;
	return true;
}

void Burst::GlobalBufferSize::free(uint64_t size)
{
	Poco::FastMutex::ScopedLock lock{ mutex_ };
	
	if (size > size_)
		size = size_;

	size_ -= size;
}

uint64_t Burst::GlobalBufferSize::getSize() const
{
	return size_;
}

uint64_t Burst::GlobalBufferSize::getMax() const
{
	return max_;
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

		// put in all related plot files
		for (const auto& relatedPlotList : plotReadNotification->relatedPlotLists)
			for (const auto& relatedPlotFile : relatedPlotList.second)
				plotReadNotification->plotList.emplace_back(relatedPlotFile);

		for (auto plotFileIter = plotReadNotification->plotList.begin();
			plotFileIter != plotReadNotification->plotList.end() &&
			!isCancelled();
			++plotFileIter)
		{
			auto& plotFile = **plotFileIter;
			std::ifstream inputStream(plotFile.getPath(), std::ifstream::in | std::ifstream::binary);
			
			Poco::Timestamp timeStartFile;

			if (!isCancelled() && inputStream.is_open())
			{
				auto accountId = stoull(getAccountIdFromPlotFile(plotFile.getPath()));
				auto nonceStart = stoull(getStartNonceFromPlotFile(plotFile.getPath()));
				auto nonceCount = stoull(getNonceCountFromPlotFile(plotFile.getPath()));
				auto staggerSize = stoull(getStaggerSizeFromPlotFile(plotFile.getPath()));

				//MinerLogger::write("reading plot file " + plotFile.getPath());

				const auto staggerBlocks = nonceCount / staggerSize;
				const auto staggerBlockSize = staggerSize * Settings::PlotSize;
				const auto staggerScoopBytes = staggerSize * Settings::ScoopSize;
				unsigned long long int templateArg = 0;
				const auto staggerChunkBytes = [staggerScoopBytes](auto arg)
				{
					decltype(arg) a = MinerConfig::getConfig().maxBufferSizeMB * 1024 * 1024;
					decltype(arg) b = staggerScoopBytes;

					for(;;) 
					{ 
						if (a == 0) // trap if a==0 
							return b;

						b %= a; // otherwise here would be an error 
						
						if (b == 0) // trap if b==0 
							return a;

						a %= b; // otherwise here would be an error 
					}

					return decltype(arg)();
				}(templateArg);
				const auto staggerChunks = staggerScoopBytes / staggerChunkBytes;
				const auto staggerChunkSize = staggerSize / staggerChunks;

				for (auto staggerBlock = 0ul; !isCancelled() && staggerBlock < staggerBlocks; ++staggerBlock)
				{
					const auto staggerBlockOffset = staggerBlock * staggerBlockSize;
					const auto staggerScoopOffset = plotReadNotification->scoopNum * staggerSize * Settings::ScoopSize;

					for (auto staggerChunk = 0u; staggerChunk < staggerChunks && !isCancelled(); ++staggerChunk)
					{
						auto memoryAcquired = false;

						while (!isCancelled() && !memoryAcquired)
							memoryAcquired = globalBufferSize.reserve(staggerChunkBytes);

						if (!isCancelled() && memoryAcquired)
						{
							const auto chunkOffset = staggerChunk * staggerChunkBytes;

							VerifyNotification::Ptr verification = new VerifyNotification{};
							verification->accountId = accountId;
							verification->nonceStart = nonceStart;
							verification->block = plotReadNotification->blockheight;
							verification->inputPath = plotFile.getPath();
							verification->gensig = plotReadNotification->gensig;
							verification->buffer.resize(static_cast<size_t>(staggerChunkSize));
							verification->nonceRead = staggerBlock * staggerSize + staggerChunkSize * staggerChunk;
							verification->baseTarget = plotReadNotification->baseTarget;

							inputStream.seekg(staggerBlockOffset + staggerScoopOffset + chunkOffset);
							//inputStream.read(reinterpret_cast<char*>(&verification->buffer[0]), staggerScoopSize);
							inputStream.read(reinterpret_cast<char*>(&verification->buffer[0]), staggerChunkBytes);

							verificationQueue_->enqueueNotification(verification);
						}
					}
				}

				inputStream.close();

				if (!isCancelled())
				{
					if (progress_ != nullptr)
					{
						progress_->add(plotFile.getSize(), plotReadNotification->blockheight);
						miner_.getData().getBlockData()->setProgress(progress_->getProgress(), plotReadNotification->blockheight);
					}

					auto fileReadDiff = timeStartFile.elapsed();
					auto fileReadDiffSeconds = static_cast<float>(fileReadDiff) / 1000 / 1000;
					Poco::Timespan span{fileReadDiff};

					const auto nonceBytes = static_cast<float>(nonceCount * Settings::ScoopSize);
					const auto bytesPerSeconds = nonceBytes / fileReadDiffSeconds;

					log_information_if(MinerLogger::plotReader, MinerLogger::hasOutput(PlotDone), "%s (%s) read in %ss (~%s/s)",
						plotFile.getPath(),
						memToString(plotFile.getSize(), 2),
						Poco::DateTimeFormatter::format(span, "%s.%i"),
						memToString(static_cast<uint64_t>(bytesPerSeconds), 2));
				}
			}
		}
		
		auto dirReadDiff = timeStartDir.elapsed();
		auto dirReadDiffSeconds = static_cast<float>(dirReadDiff) / 1000 / 1000;
		Poco::Timespan span{dirReadDiff};

		uint64_t totalSizeBytes = 0u;

		for (const auto& plot : plotReadNotification->plotList)
			totalSizeBytes += plot->getSize();
		
		if (plotReadNotification->type == PlotDir::Type::Sequential)
		{
			const auto sumNonces = totalSizeBytes / Settings::PlotSize;
			const auto sumNoncesBytes = static_cast<float>(sumNonces * Settings::ScoopSize);
			const auto bytesPerSecond = sumNoncesBytes / dirReadDiffSeconds;
			
			std::stringstream sstr;

			sstr << plotReadNotification->dir;

			for (const auto& relatedPlotList : plotReadNotification->relatedPlotLists)
				sstr << " + " << relatedPlotList.first;

			log_information_if(MinerLogger::plotReader, MinerLogger::hasOutput(DirDone), "Dir %s read (%z files, %s total) in %ss (~%s/s)",
				sstr.str(),
				plotReadNotification->plotList.size(),
				memToString(totalSizeBytes, 2),
				Poco::DateTimeFormatter::format(span, "%s.%i"),
				memToString(static_cast<uint64_t>(bytesPerSecond), 2));
		}
	}
}

void Burst::PlotReadProgress::reset(uint64_t blockheight, uintmax_t max)
{
	Poco::Mutex::ScopedLock guard(lock_);
	progress_ = 0;
	blockheight_ = blockheight;
	max_ = max;
}

void Burst::PlotReadProgress::add(uintmax_t value, uint64_t blockheight)
{
	Poco::Mutex::ScopedLock guard(lock_);

	if (blockheight != blockheight_)
		return;

	progress_ += value;

	if (max_ > 0)
		MinerLogger::writeProgress(getProgress(), 48);
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
