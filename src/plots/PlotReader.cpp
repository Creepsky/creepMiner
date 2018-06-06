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
#include <Poco/FileStream.h>

#define _FILE_OFFSET_BITS 64

#if defined __APPLE__
#define LSEEK64 lseek
#elif defined __linux__
#define LSEEK64 lseek64
#endif

Burst::GlobalBufferSize Burst::PlotReader::globalBufferSize;

void Burst::GlobalBufferSize::setMax(const Poco::UInt64 max)
{
	chunkQueue.wakeUpAll();

	if (max > 0)
	{
		if (getMax() == max)
			return;

		if (memoryPool_ != nullptr)
			memoryPool_.reset();

		const auto chunks = MinerConfig::getConfig().getBufferChunkCount();
		auto chunkSize = max / chunks;

		memoryPool_ = std::make_unique<Poco::MemoryPool>(chunkSize, 0, chunks);
		max_ = max;
	}
}

void* Burst::GlobalBufferSize::reserve()
{
	// unlimited memory
	//if (MinerConfig::getConfig().getMaxBufferSizeRaw() == 0)
	//	return true;

	try
	{
		return memoryPool_->get();
	}
	catch (...)
	{
		return nullptr;
	}
}

void Burst::GlobalBufferSize::free(void* memory)
{
	// unlimited memory
	//if (MinerConfig::getConfig().getMaxBufferSizeRaw() == 0)
	//	return true;

	memoryPool_->release(memory);
}

Poco::UInt64 Burst::GlobalBufferSize::getSize() const
{
	if (memoryPool_ == nullptr)
		return 0;

	return memoryPool_->blockSize() * memoryPool_->allocated();
}

Poco::UInt64 Burst::GlobalBufferSize::getMax() const
{
	return max_;
}

Burst::PlotReader::PlotReader(MinerData& data, std::shared_ptr<PlotReadProgress> progressRead,
                              std::shared_ptr<PlotReadProgress> progressVerify,
                              Poco::NotificationQueue& verificationQueue, Poco::NotificationQueue& plotReadQueue)
	: Task("PlotReader"), data_(data), progressRead_{std::move(progressRead)}, progressVerify_{std::move(progressVerify)},
	  verificationQueue_{&verificationQueue},
	  plotReadQueue_(&plotReadQueue)
{
}

void Burst::PlotReader::runTask()
{
	ScoopData* memoryMirror = nullptr;

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
				PlotReadProgressGuard progressGuard{progressRead_, plotFile.getNonces(), plotReadNotification->blockheight};
				const auto progressGuardVerify = std::make_shared<PlotReadProgressGuard>(
					progressVerify_, plotFile.getNonces(), plotReadNotification->blockheight);

#ifdef _WIN32
				DWORD _;
				const auto inputStream = CreateFileA(plotFile.getPath().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
				                                     OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, nullptr);
#else
				const auto inputStream = open(plotFile.getPath().c_str(), O_RDONLY);
#endif

				Poco::Timestamp timeStartFile;

#ifdef _WIN32
				if (!isCancelled() && inputStream != INVALID_HANDLE_VALUE)
#else
				if (!isCancelled() && inputStream >= 0)
#endif
				{
					if (plotReadNotification->wakeUpCall)
					{
						// its just a wake up call for the HDD, simply read the first byte
						char dummyByte;
						//
#ifdef _WIN32
						ReadFile(inputStream, &dummyByte, 1, &_, nullptr);
						CloseHandle(inputStream);
#else
						if (read(inputStream, &dummyByte, 1) != 1)
							log_error(MinerLogger::plotReader, "Could not wake up HDD %s", plotReadNotification->dir);

						close(inputStream);
#endif

						log_debug(MinerLogger::plotReader, "Woke up the HDD %s", plotReadNotification->dir);

						// ... and then jump to the next notification, no need to search for deadlines
						break;
					}

					const auto maxBufferSize = MinerConfig::getConfig().getMaxBufferSizeRaw();
					auto chunkBytes = MinerConfig::getConfig().getMaxBufferSize() / MinerConfig::getConfig().getBufferChunkCount();

					// unlimited buffer size
					if (maxBufferSize == 0)
						chunkBytes = plotFile.getStaggerScoopBytes();

					const auto noncesPerChunk = std::min(chunkBytes / Settings::scoopSize, plotFile.getStaggerSize());
					auto nonce = 0ull;

					while (nonce < plotFile.getNonces() && currentBlock && !isCancelled())
					{
						const auto startNonce = nonce;
						auto readNonces = noncesPerChunk;
						const auto staggerBegin = startNonce / plotFile.getStaggerSize();
						const auto staggerEnd = (startNonce + readNonces) / plotFile.getStaggerSize();

						if (staggerBegin != staggerEnd)
							readNonces = plotFile.getStaggerSize() - startNonce % plotFile.getStaggerSize();

						const auto memoryToAcquire = std::min(readNonces * Settings::scoopSize, chunkBytes);
						ScoopData* memory = nullptr;

						while (!isCancelled() && memory == nullptr)
						{
							memory = reinterpret_cast<ScoopData*>(globalBufferSize.reserve());

							if (poc2 && !plotFile.isPoC(2))
								while (!isCancelled() && memoryMirror == nullptr)
									memoryMirror = reinterpret_cast<ScoopData*>(globalBufferSize.reserve());							
						}

						// if the reader is cancelled, jump out of the loop
						if (isCancelled())
						{
							// but first give free the allocated memory
							if (memory != nullptr)
								globalBufferSize.free(memory);

							if (memoryMirror != nullptr)
								globalBufferSize.free(memoryMirror);

							continue;
						}

						if (memory != nullptr && currentBlock)
						{
							const auto chunkOffset = startNonce % plotFile.getStaggerSize() * Settings::scoopSize;
							const auto staggerBlockOffset = staggerBegin * plotFile.getStaggerBytes();
							const auto staggerScoopOffset = plotReadNotification->scoopNum * plotFile.getStaggerScoopBytes();

							VerifyNotification::Ptr verification(new VerifyNotification{});
							verification->accountId = plotFile.getAccountId();
							verification->nonceStart = plotFile.getNonceStart();
							verification->block = plotReadNotification->blockheight;
							verification->inputPath = plotFile.getPath();
							verification->gensig = plotReadNotification->gensig;
							verification->nonceRead = startNonce;
							verification->baseTarget = plotReadNotification->baseTarget;
							verification->nonces = readNonces;
							verification->buffer = memory;
							verification->progress = progressGuardVerify;

#ifdef _WIN32
							LARGE_INTEGER winOffset;
							winOffset.QuadPart = static_cast<LONGLONG>(staggerBlockOffset + staggerScoopOffset + chunkOffset);
							SetFilePointerEx(inputStream, winOffset, nullptr, FILE_BEGIN);
							ReadFile(inputStream, reinterpret_cast<char*>(verification->buffer), memoryToAcquire, &_, nullptr);
#else
							LSEEK64(inputStream, staggerBlockOffset + staggerScoopOffset + chunkOffset, SEEK_SET);
							if (read(inputStream, reinterpret_cast<char*>(verification->buffer), memoryToAcquire) != memoryToAcquire)
								log_warning(MinerLogger::plotReader, "Could not read nonce %Lu+ in %s", startNonce, plotFile.getPath());
#endif

							if (memoryMirror != nullptr)
							{
								const auto staggerScoopOffsetMirror = (4095 - plotReadNotification->scoopNum) * plotFile.getStaggerScoopBytes();
#ifdef _WIN32
								LARGE_INTEGER winOffsetMirror;
								winOffsetMirror.QuadPart = static_cast<LONGLONG>(staggerBlockOffset + staggerScoopOffsetMirror + chunkOffset);
								SetFilePointerEx(inputStream, winOffsetMirror, nullptr, FILE_BEGIN);
								ReadFile(inputStream, reinterpret_cast<char*>(memoryMirror), memoryToAcquire, &_, nullptr);
#else
								LSEEK64(inputStream, staggerBlockOffset + staggerScoopOffsetMirror + chunkOffset, SEEK_SET);
								if (read(inputStream, reinterpret_cast<char*>(memoryMirror), memoryToAcquire) != memoryToAcquire)
									log_warning(MinerLogger::plotReader, "Could not read mirror for nonce %Lu+ in %s", startNonce, plotFile.getPath());
#endif

								for (size_t i = 0; i < readNonces; ++i)
									memcpy(&verification->buffer[i][32], &memoryMirror[i][32], 32);

								globalBufferSize.free(memoryMirror);
							}

							verificationQueue_->enqueueNotification(verification);

							// check, if the incoming plot-read-notification is for the current round
							currentBlock = plotReadNotification->blockheight == data_.getCurrentBlockheight();
							nonce += readNonces;
						}
						// if the memory was acquired, but it was not the right block, give it free
						else if (memory != nullptr)
							globalBufferSize.free(memory);
						// this should never happen.. no memory allocated, not cancelled, wrong block
						else;
					}
				}

#ifdef _WIN32
				CloseHandle(inputStream);
#else
				close(inputStream);
#endif

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

					const auto nonceBytes = static_cast<double>(plotFile.getNonces() * Settings::scoopSize);
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
				const auto sumNonces = totalSizeBytes / Settings::plotSize;
				const auto sumNoncesBytes = static_cast<float>(sumNonces * Settings::scoopSize);
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
	std::lock_guard<std::mutex> guard(mutex_);
	progress_ = 0;
	blockheight_ = blockheight;
	max_ = max;
}

void Burst::PlotReadProgress::add(uintmax_t value, Poco::UInt64 blockheight)
{
	{
		std::lock_guard<std::mutex> guard(mutex_);

		if (blockheight != blockheight_)
			return;

		progress_ += value;
	}

	fireProgressChanged();
}

bool Burst::PlotReadProgress::isReady() const
{
	std::lock_guard<std::mutex> guard(mutex_);
	return progress_ >= max_;
}

uintmax_t Burst::PlotReadProgress::getValue() const
{
	std::lock_guard<std::mutex> guard(mutex_);
	return progress_;
}

float Burst::PlotReadProgress::getProgress() const
{
	std::lock_guard<std::mutex> guard(mutex_);

	if (max_ == 0.f)
		return 0.f;

	return progress_ * 1.f / max_ * 100;
}

void Burst::PlotReadProgress::fireProgressChanged()
{
	auto progress = getProgress();
	progressChanged.notify(this, progress);
}

Burst::PlotReadProgressGuard::PlotReadProgressGuard(std::shared_ptr<PlotReadProgress> progress, const Poco::UInt64 nonces,
	const Poco::UInt64 blockheight)
	: progress_(std::move(progress)), nonces_(nonces), blockheight_(blockheight)
{
}

Burst::PlotReadProgressGuard::~PlotReadProgressGuard()
{
	progress_->add(nonces_ * Settings::plotSize, blockheight_);
}
