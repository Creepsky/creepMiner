// ==========================================================================
// 
// creepMiner - Burstcoin cryptocurrency CPU and GPU miner
// Copyright (C)  2016-2018 Creepsky (creepsky@gmail.com)
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110 - 1301  USA
// 
// ==========================================================================

#include "PlotReader.hpp"
#include "MinerUtil.hpp"
#include "logging/MinerLogger.hpp"
#include "mining/MinerConfig.hpp"
#include <fstream>
#include <utility>
#include "mining/Miner.hpp"
#include <Poco/NotificationQueue.h>
#include "PlotVerifier.hpp"
#include <Poco/Timestamp.h>
#include "logging/Output.hpp"
#include "Plot.hpp"
#include "logging/Performance.hpp"

Burst::GlobalBufferSize Burst::PlotReader::globalBufferSize;

void Burst::GlobalBufferSize::setMax(const Poco::UInt64 max)
{
	Poco::Mutex::ScopedLock lock{mutex_};
	max_ = max;
}

bool Burst::GlobalBufferSize::reserve(const Poco::UInt64 size)
{
	// unlimited memory
	if (MinerConfig::getConfig().getMaxBufferSize() == 0)
		return true;

	Poco::Mutex::ScopedLock lock{mutex_};

	if (size_ + size > max_)
		return false;

	size_ += size;
	return true;
}

void Burst::GlobalBufferSize::free(Poco::UInt64 size)
{
	// unlimited memory
	if (MinerConfig::getConfig().getMaxBufferSize() == 0)
		return;

	Poco::Mutex::ScopedLock lock{mutex_};

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

Burst::PlotReader::PlotReader(MinerData& data, std::shared_ptr<PlotReadProgress> progress,
                              Poco::NotificationQueue& verificationQueue, Poco::NotificationQueue& plotReadQueue)
	: Task("PlotReader"), data_(data), progress_{std::move(progress)}, verificationQueue_{&verificationQueue},
	  plotReadQueue_(&plotReadQueue)
{
}

void Burst::PlotReader::runTask()
{
	std::vector<ScoopData> bufferMirror;

	while (!isCancelled())
	{
		try
		{
			Poco::Notification::Ptr notification(plotReadQueue_->waitDequeueNotification());
			PlotReadNotification::Ptr plotReadNotification;

			if (notification)
				plotReadNotification = notification.cast<PlotReadNotification>();
			else
				break;

			START_PROBE_DOMAIN("PlotReader.ReadDir", plotReadNotification->dir)

			// only process the current block
			if (data_.getCurrentBlockheight() != plotReadNotification->blockheight)
				continue;

			auto poc2 = false;

			if (MinerConfig::getConfig().getPoc2StartBlock() > 0)
				poc2 = MinerConfig::getConfig().getPoc2StartBlock() <= plotReadNotification->blockheight;

			Poco::Timestamp timeStartDir;

			// check, if the incoming plot-read-notification is for the current round
			auto currentBlock = plotReadNotification->blockheight == data_.getCurrentBlockheight();
			auto& plotList = plotReadNotification->plotList;

			// put in all related plot files
			for (const auto& relatedPlotList : plotReadNotification->relatedPlotLists)
				for (const auto& relatedPlotFile : relatedPlotList.second)
					plotList.emplace_back(relatedPlotFile);

			for (auto plotFileIter = plotList.begin(); plotFileIter != plotList.end() && !isCancelled() && currentBlock; ++plotFileIter)
			{
				auto& plotFile = **plotFileIter;
				std::ifstream inputStream(plotFile.getPath(), std::ifstream::in | std::ifstream::binary);

				START_PROBE_DOMAIN("PlotReader.ReadFile", plotFile.getPath())
				Poco::Timestamp timeStartFile;

				if (!isCancelled() && inputStream.is_open())
				{
					if (plotReadNotification->wakeUpCall)
					{
						// its just a wake up call for the HDD, simply read the first byte
						char dummyByte;
						//
						inputStream.read(&dummyByte, 1);

						// close the file ...
						inputStream.close();

						log_debug(MinerLogger::plotReader, "Woke up the HDD %s", plotReadNotification->dir);

						// ... and then jump to the next notification, no need to search for deadlines
						break;
					}

					const auto maxBufferSize = MinerConfig::getConfig().getMaxBufferSize();
					auto chunkBytes = maxBufferSize / MinerConfig::getConfig().getBufferChunkCount();

					// unlimited buffer size
					if (maxBufferSize == 0)
						chunkBytes = plotFile.getStaggerScoopBytes();

					const auto noncesPerChunk = std::min(chunkBytes / Settings::ScoopSize, plotFile.getStaggerSize());
					auto nonce = 0ull;

					while (nonce < plotFile.getNonces() && currentBlock && !isCancelled())
					{
						START_PROBE_DOMAIN("PlotReader.Nonces", plotFile.getPath());
						const auto startNonce = nonce;
						auto readNonces = noncesPerChunk;
						const auto staggerBegin = startNonce / plotFile.getStaggerSize();
						const auto staggerEnd = (startNonce + readNonces) / plotFile.getStaggerSize();

						if (staggerBegin != staggerEnd)
							readNonces = plotFile.getStaggerSize() - startNonce % plotFile.getStaggerSize();

						auto memoryAcquired = false;
						auto memoryAcquiredMirror = false;
						const auto memoryToAcquire = std::min(readNonces * Settings::ScoopSize, chunkBytes);

						START_PROBE_DOMAIN("PlotReader.AllocMemory", plotFile.getPath());
						while (!isCancelled() && !memoryAcquired)
						{
							memoryAcquired = globalBufferSize.reserve(memoryToAcquire);

							if ((poc2 && !plotFile.isPoC(2)) || (!poc2 && plotFile.isPoC(2)))
								while (!isCancelled() && !memoryAcquiredMirror)
									memoryAcquiredMirror = globalBufferSize.reserve(memoryToAcquire);
						}
						TAKE_PROBE_DOMAIN("PlotReader.AllocMemory", plotFile.getPath());

						// if the reader is cancelled, jump out of the loop
						if (isCancelled())
						{
							// but first give free the allocated memory
							if (memoryAcquired)
								globalBufferSize.free(memoryToAcquire);

							if (memoryAcquiredMirror)
								globalBufferSize.free(memoryToAcquire);

							continue;
						}

						if (memoryAcquired && currentBlock)
						{
							START_PROBE_DOMAIN("PlotReader.PushWork", plotFile.getPath());
							const auto chunkOffset = startNonce % plotFile.getStaggerSize() * Settings::ScoopSize;
							const auto staggerBlockOffset = staggerBegin * plotFile.getStaggerBytes();
							const auto staggerScoopOffset = plotReadNotification->scoopNum * plotFile.getStaggerScoopBytes();

							START_PROBE("PlotReader.CreateVerification");
							VerifyNotification::Ptr verification(new VerifyNotification{});
							verification->accountId = plotFile.getAccountId();
							verification->nonceStart = plotFile.getNonceStart();
							verification->block = plotReadNotification->blockheight;
							verification->inputPath = plotFile.getPath();
							verification->gensig = plotReadNotification->gensig;
							verification->nonceRead = startNonce;
							verification->baseTarget = plotReadNotification->baseTarget;
							verification->memorySize = memoryToAcquire;

							memoryAcquired = false;

							while (!memoryAcquired && !isCancelled())
							{
								try
								{
									verification->buffer.resize(readNonces);
									memoryAcquired = true;
								}
								catch (std::bad_alloc&)
								{
								}
								catch (...)
								{
									globalBufferSize.free(memoryToAcquire);
									throw;
								}
							}

							if (memoryAcquiredMirror)
							{
								memoryAcquiredMirror = false;

								while (!memoryAcquiredMirror && !isCancelled())
								{
									try
									{
										bufferMirror.resize(readNonces);
										memoryAcquiredMirror = true;
									}
									catch (std::bad_alloc&)
									{
									}
									catch (...)
									{
										globalBufferSize.free(memoryToAcquire);
										throw;
									}
								}
							}
							TAKE_PROBE("PlotReader.CreateVerification");

							START_PROBE_DOMAIN("PlotReader.SeekAndRead", plotFile.getPath());
							inputStream.seekg(staggerBlockOffset + staggerScoopOffset + chunkOffset);
							inputStream.read(reinterpret_cast<char*>(&verification->buffer[0]), memoryToAcquire);

							if (memoryAcquiredMirror)
							{
								const auto staggerScoopOffsetMirror = (4095 - plotReadNotification->scoopNum) * plotFile.getStaggerScoopBytes();
								inputStream.seekg(staggerBlockOffset + staggerScoopOffsetMirror + chunkOffset);
								inputStream.read(reinterpret_cast<char*>(&bufferMirror[0]), memoryToAcquire);

								for (size_t i = 0; i < verification->buffer.size(); ++i)
									memcpy(&verification->buffer[i][32], &bufferMirror[i][32], 32);

								bufferMirror.clear();
								globalBufferSize.free(memoryToAcquire);
							}
							TAKE_PROBE_DOMAIN("PlotReader.SeekAndRead", plotFile.getPath());

							verificationQueue_->enqueueNotification(verification);

							if (MinerConfig::getConfig().isSteadyProgressBar() && progress_ != nullptr)
								progress_->add(readNonces * Settings::PlotSize, plotReadNotification->blockheight);

							// check, if the incoming plot-read-notification is for the current round
							currentBlock = plotReadNotification->blockheight == data_.getCurrentBlockheight();
							nonce += readNonces;

							TAKE_PROBE_DOMAIN("PlotReader.PushWork", plotFile.getPath());
						}
						// if the memory was acquired, but it was not the right block, give it free
						else if (memoryAcquired)
							globalBufferSize.free(memoryToAcquire);
							// this should never happen.. no memory allocated, not cancelled, wrong block
						else;
						TAKE_PROBE_DOMAIN("PlotReader.Nonces", plotFile.getPath());
					}
				}

				inputStream.close();

				// check, if the incoming plot-read-notification is for the current round
				currentBlock = plotReadNotification->blockheight == data_.getCurrentBlockheight();

				if (!isCancelled() && currentBlock)
				{
					const auto fileReadDiff = timeStartFile.elapsed();
					const auto fileReadDiffSeconds = static_cast<float>(fileReadDiff) / 1000 / 1000;
					const Poco::Timespan span{fileReadDiff};

					const auto plotListSize = plotReadNotification->plotList.size();

					if (plotListSize > 0)
					{
						data_.getBlockData()->setProgress(
							plotReadNotification->dir,
							static_cast<float>(std::distance(plotReadNotification->plotList.begin(), plotFileIter) + 1) / plotListSize
							* 100.f,
							plotReadNotification->blockheight
						);
					}

					const auto nonceBytes = static_cast<double>(plotFile.getNonces() * Settings::ScoopSize);
					const auto bytesPerSeconds = nonceBytes / fileReadDiffSeconds;

					log_information_if(MinerLogger::plotReader, MinerLogger::hasOutput(PlotDone), "%s (%s) read in %ss (~%s/s)",
						plotFile.getPath(),
						memToString(plotFile.getSize(), 2),
						Poco::DateTimeFormatter::format(span, "%s.%i"),
						memToString(static_cast<Poco::UInt64>(bytesPerSeconds), 2));

					if (!MinerConfig::getConfig().isSteadyProgressBar() && progress_ != nullptr)
					{
						START_PROBE("PlotReader.Progress")
						progress_->add(plotFile.getSize(), plotReadNotification->blockheight);
						TAKE_PROBE("PlotReader.Progress")
					}
				}

				// if it was cancelled, we push the current plot dir back in the queue again
				if (isCancelled())
					plotReadQueue_->enqueueNotification(plotReadNotification);

				TAKE_PROBE_DOMAIN("PlotReader.ReadFile", plotFile.getPath());
			}

			if (plotReadNotification->wakeUpCall)
				continue;

			data_.getBlockData()->setProgress(plotReadNotification->dir, 100.f, plotReadNotification->blockheight);

			const auto dirReadDiff = timeStartDir.elapsed();
			const auto dirReadDiffSeconds = static_cast<float>(dirReadDiff) / 1000 / 1000;
			const Poco::Timespan span{dirReadDiff};

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

				log_information_if(MinerLogger::plotReader, MinerLogger::hasOutput(DirDone),
					"Dir %s read in %ss (~%s/s)\n"
					"\t%z %s (%s)",
					sstr.str(),
					Poco::DateTimeFormatter::format(span, "%s.%i"),
					memToString(static_cast<Poco::UInt64>(bytesPerSecond), 2),
					plotReadNotification->plotList.size(),
					plotReadNotification->plotList.size() == 1 ? std::string("file") : std::string("files"),
					memToString(totalSizeBytes, 2));
			}

			TAKE_PROBE_DOMAIN("PlotReader.ReadDir", plotReadNotification->dir)
		}
		catch (Poco::Exception& exc)
		{
			log_fatal(MinerLogger::plotReader, "A plotreader just crashed while reading!");
			log_exception(MinerLogger::plotReader, exc);
		}
		catch (std::exception& exc)
		{
			log_fatal(MinerLogger::plotReader, "A plotreader just crashed while reading!\n"
				"\tReason: %s", std::string(exc.what()));
		}
	}
}

void Burst::PlotReadProgress::reset(Poco::UInt64 blockheight, uintmax_t max)
{
	Poco::Mutex::ScopedLock guard(mutex_);
	progress_ = 0;
	blockheight_ = blockheight;
	max_ = max;
}

void Burst::PlotReadProgress::add(uintmax_t value, Poco::UInt64 blockheight)
{
	{
		Poco::Mutex::ScopedLock guard(mutex_);

		if (blockheight != blockheight_)
			return;

		progress_ += value;
	}

	fireProgressChanged();
}

bool Burst::PlotReadProgress::isReady() const
{
	Poco::Mutex::ScopedLock guard(mutex_);
	return progress_ >= max_;
}

uintmax_t Burst::PlotReadProgress::getValue() const
{
	Poco::Mutex::ScopedLock guard(mutex_);
	return progress_;
}

float Burst::PlotReadProgress::getProgress() const
{
	Poco::Mutex::ScopedLock guard(mutex_);

	if (max_ == 0.f)
		return 0.f;

	return progress_ * 1.f / max_ * 100;
}

void Burst::PlotReadProgress::fireProgressChanged()
{
	auto progress = getProgress();
	progressChanged.notify(this, progress);
}
