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

void Burst::GlobalBufferSize::setMax(Poco::UInt64 max)
{
	Poco::FastMutex::ScopedLock lock{ mutex_ };
	max_ = max;
}

bool Burst::GlobalBufferSize::reserve(Poco::UInt64 size)
{
	Poco::FastMutex::ScopedLock lock{ mutex_ };
	
	if (size_ + size > max_)
		return false;

	size_ += size;
	return true;
}

void Burst::GlobalBufferSize::free(Poco::UInt64 size)
{
	Poco::FastMutex::ScopedLock lock{ mutex_ };
	
	if (size > size_)
		size = size_;

	size_ -= size;
}

Poco::UInt64 Burst::GlobalBufferSize::getSize() const
{
	return size_;
}

Poco::UInt64 Burst::GlobalBufferSize::getMax() const
{
	return max_;
}

Burst::PlotReader::PlotReader(Miner& miner, std::shared_ptr<Burst::PlotReadProgress> progress,
	Poco::NotificationQueue& verificationQueue, Poco::NotificationQueue& plotReadQueue)
	: Task("PlotReader"), miner_(miner), progress_{progress}, verificationQueue_{&verificationQueue}, plotReadQueue_(&plotReadQueue)
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

		// only process the current block
		if (miner_.getBlockheight() != plotReadNotification->blockheight)
			continue;

		Poco::Timestamp timeStartDir;

		// check, if the incoming plot-read-notification is for the current round
		auto currentBlock = plotReadNotification->blockheight == miner_.getBlockheight();

		// put in all related plot files
		for (const auto& relatedPlotList : plotReadNotification->relatedPlotLists)
			for (const auto& relatedPlotFile : relatedPlotList.second)
				plotReadNotification->plotList.emplace_back(relatedPlotFile);

		for (auto plotFileIter = plotReadNotification->plotList.begin();
			plotFileIter != plotReadNotification->plotList.end() &&
			!isCancelled() &&
			currentBlock;
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
				const Poco::UInt64 staggerChunkBytes = [staggerScoopBytes]()
				{
					Poco::UInt64 a = MinerConfig::getConfig().getMaxBufferSize() * 1024 * 1024;
					Poco::UInt64 b = staggerScoopBytes;

					if (a > b)
						return b;

					for(;;) 
					{ 
						if (a == 0) // trap if a==0 
							return b;

						b %= a; // otherwise here would be an error 
						
						if (b == 0) // trap if b==0 
							return a;

						a %= b; // otherwise here would be an error 
					}

					return Poco::UInt64();
				}();
				const auto staggerChunks = staggerScoopBytes / staggerChunkBytes;
				const auto staggerChunkSize = staggerSize / staggerChunks;

				// check, if the incoming plot-read-notification is for the current round
				currentBlock = plotReadNotification->blockheight == miner_.getBlockheight();

				for (auto staggerBlock = 0ul; !isCancelled() && staggerBlock < staggerBlocks && currentBlock; ++staggerBlock)
				{
					const auto staggerBlockOffset = staggerBlock * staggerBlockSize;
					const auto staggerScoopOffset = plotReadNotification->scoopNum * staggerSize * Settings::ScoopSize;

					// check, if the incoming plot-read-notification is for the current round
					currentBlock = plotReadNotification->blockheight == miner_.getBlockheight();

					for (auto staggerChunk = 0u; staggerChunk < staggerChunks && !isCancelled() && currentBlock; ++staggerChunk)
					{
						auto memoryAcquired = false;

						while (!isCancelled() && !memoryAcquired && currentBlock)
						{
							memoryAcquired = globalBufferSize.reserve(staggerChunkBytes);

							// check, if the incoming plot-read-notification is for the current round
							// this could actually be very heavy on performance, maybe it should be
							// called every x loops
							currentBlock = plotReadNotification->blockheight == miner_.getBlockheight();
						}

						// if the reader is cancelled, jump out of the loop
						if (isCancelled())
						{
							// but first give free the allocated memory
							if (memoryAcquired)
								globalBufferSize.free(staggerChunkBytes);

							continue;
						}

						// only send the data to the verifiers, if the memory could be acquired and it is the right block
						if (memoryAcquired && currentBlock)
						{
							const auto chunkOffset = staggerChunk * staggerChunkBytes;

							VerifyNotification::Ptr verification(new VerifyNotification{});
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

							if (progress_ != nullptr)
							{
								progress_->add(staggerChunkBytes * Settings::ScoopPerPlot, plotReadNotification->blockheight);
								miner_.getData().getBlockData()->setProgress(progress_->getProgress(), plotReadNotification->blockheight);
							}

							// check, if the incoming plot-read-notification is for the current round
							currentBlock = plotReadNotification->blockheight == miner_.getBlockheight();
						}
						// if the memory was acquired, but it was not the right block, give it free
						else if (memoryAcquired)
							globalBufferSize.free(staggerChunkBytes);
						// this should never happen.. no memory allocated, not cancelled, wrong block
						else;
					}
				}

				inputStream.close();

				// check, if the incoming plot-read-notification is for the current round
				currentBlock = plotReadNotification->blockheight == miner_.getBlockheight();

				if (!isCancelled() && currentBlock)
				{
					auto fileReadDiff = timeStartFile.elapsed();
					auto fileReadDiffSeconds = static_cast<float>(fileReadDiff) / 1000 / 1000;
					Poco::Timespan span{fileReadDiff};
				
    				auto plotListSize = plotReadNotification->plotList.size();
    
    				if (plotListSize > 0)
    					miner_.getData().getBlockData()->setProgress(
							plotReadNotification->dir,
    						static_cast<float>(std::distance(plotReadNotification->plotList.begin(), plotFileIter) + 1) / plotListSize * 100.f,
							plotReadNotification->blockheight
						);

					const auto nonceBytes = static_cast<float>(nonceCount * Settings::ScoopSize);
					const auto bytesPerSeconds = nonceBytes / fileReadDiffSeconds;

					log_information_if(MinerLogger::plotReader, MinerLogger::hasOutput(PlotDone), "%s (%s) read in %ss (~%s/s)",
						plotFile.getPath(),
						memToString(plotFile.getSize(), 2),
						Poco::DateTimeFormatter::format(span, "%s.%i"),
						memToString(static_cast<Poco::UInt64>(bytesPerSeconds), 2));
				}

				// if it was cancelled, we push the current plot dir back in the queue again
				if (isCancelled())
					plotReadQueue_->enqueueNotification(plotReadNotification);
			}
		}
		
		miner_.getData().getBlockData()->setProgress(plotReadNotification->dir, 100.f, plotReadNotification->blockheight);

		auto dirReadDiff = timeStartDir.elapsed();
		auto dirReadDiffSeconds = static_cast<float>(dirReadDiff) / 1000 / 1000;
		Poco::Timespan span{dirReadDiff};

		Poco::UInt64 totalSizeBytes = 0u;

		for (const auto& plot : plotReadNotification->plotList)
			totalSizeBytes += plot->getSize();
		
		if (plotReadNotification->type == PlotDir::Type::Sequential && totalSizeBytes > 0 && currentBlock)
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
				memToString(static_cast<Poco::UInt64>(bytesPerSecond), 2));
		}
	}
}

void Burst::PlotReadProgress::reset(Poco::UInt64 blockheight, uintmax_t max)
{
	Poco::Mutex::ScopedLock guard(lock_);
	progress_ = 0;
	blockheight_ = blockheight;
	max_ = max;
}

void Burst::PlotReadProgress::add(uintmax_t value, Poco::UInt64 blockheight)
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
