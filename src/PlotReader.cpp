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

std::atomic_uint64_t Burst::PlotReader::PlotReader::sumBufferSize_ = 0;

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

		for (auto plotFileIter = plotReadNotification->plotList.begin();
			plotFileIter != plotReadNotification->plotList.end() &&
			!isCancelled() &&
			miner_.getBlockheight() == plotReadNotification->blockheight;
			++plotFileIter)
		{
			auto& plotFile = *(*plotFileIter);
			uint64_t nonceRead = 0;
			std::ifstream inputStream(plotFile.getPath(), std::ifstream::binary);

			Poco::Timestamp timeStartFile;

			if (inputStream.good())
			{
				auto accountId = stoull(getAccountIdFromPlotFile(plotFile.getPath()));
				auto nonceStart = stoull(getStartNonceFromPlotFile(plotFile.getPath()));
				auto nonceCount = stoull(getNonceCountFromPlotFile(plotFile.getPath()));
				auto staggerSize = stoull(getStaggerSizeFromPlotFile(plotFile.getPath()));
				auto block = miner_.getBlockheight();
				auto gensig = miner_.getGensig();

				size_t chunkNum = 0;
				auto totalChunk = static_cast<size_t>(std::ceil(static_cast<double>(nonceCount) / static_cast<double>(staggerSize)));

				//MinerLogger::write("reading plot file " + plotFile.getPath());

				while (!isCancelled() &&
					miner_.getBlockheight() == plotReadNotification->blockheight &&
					inputStream.good() &&
					chunkNum <= totalChunk)
				{
					auto scoopBufferSize = MinerConfig::getConfig().maxBufferSizeMB * 1024 * 1024 / (64 * 2); // 8192
					auto scoopBufferCount = static_cast<size_t>(
						std::ceil(static_cast<float>(staggerSize * Settings::ScoopSize) / static_cast<float>(scoopBufferSize)));
					auto startByte = plotReadNotification->scoopNum * Settings::ScoopSize * staggerSize + chunkNum * staggerSize * Settings::PlotSize;
					auto scoopDoneRead = 0u;
					size_t staggerOffset;

					while (!isCancelled() &&
						miner_.getBlockheight() == plotReadNotification->blockheight &&
						inputStream.good() &&
						scoopDoneRead <= scoopBufferCount)
					{
						// setting up plot data
						VerifyNotification::Ptr verification = new VerifyNotification{};
						verification->accountId = accountId;
						verification->nonceStart = nonceStart;
						verification->block = block;
						verification->inputPath = plotFile.getPath();
						verification->gensig = gensig;

						verification->buffer.resize(scoopBufferSize / Settings::ScoopSize);
						staggerOffset = scoopDoneRead * scoopBufferSize;

						if (scoopBufferSize > (staggerSize * Settings::ScoopSize - (scoopDoneRead * scoopBufferSize)))
						{
							scoopBufferSize = staggerSize * Settings::ScoopSize - (scoopDoneRead * scoopBufferSize);

							if (scoopBufferSize > Settings::ScoopSize)
								verification->buffer.resize(scoopBufferSize / Settings::ScoopSize);
						}

						if (scoopBufferSize > Settings::ScoopSize)
						{
							inputStream.seekg(startByte + staggerOffset);
							auto scoopData = reinterpret_cast<char*>(verification->buffer.data());
							inputStream.read(scoopData, scoopBufferSize);

							auto bufferSize = verification->buffer.size();
							verification->nonceRead = nonceRead;

							//MinerLogger::write("chunk "+std::to_string(chunkNum)+" offset "+std::to_string(startByte + staggerOffset)+" read "+std::to_string(scoopBufferSize)+" nonce offset "+std::to_string(this->nonceOffset)+" nonceRead "+std::to_string(this->nonceRead));

							// wait, when there is too much work for the verifiers
							auto waitForSpace = false;

							while (!waitForSpace &&
								!isCancelled() &&
								miner_.getBlockheight() == plotReadNotification->blockheight)
								waitForSpace = sumBufferSize_ < MinerConfig::getConfig().maxBufferSizeMB * 1024 * 1024;
							
							if (waitForSpace)
							{
								verificationQueue_->enqueueNotification(verification);
								sumBufferSize_ += bufferSize * sizeof(ScoopData);
							}

							nonceRead += bufferSize;
						}
						//else
						//{
						//	MinerLogger::write("scoop buffer ="+std::to_string(scoopBufferSize));
						//}

						++scoopDoneRead;
					}

					++chunkNum;
				}

				if (progress_ != nullptr && !isCancelled())
				{
					progress_->add(plotFile.getSize());
					miner_.getData().addBlockEntry(createJsonProgress(progress_->getProgress()));
				}

				inputStream.close();

				//MinerLogger::write("finished reading plot file " + inputPath_);
				//MinerLogger::write("plot read done. "+Burst::getFileNameFromPath(this->inputPath)+" = "+std::to_string(this->nonceRead)+" nonces ");
			}

			//if (MinerConfig::getConfig().output.debug)
				//MinerLogger::write("finished reading plot file " + plotFile.getPath(), TextType::Debug);

			auto fileReadDiff = timeStartFile.elapsed();
			auto fileReadDiffSeconds = static_cast<float>(fileReadDiff) / 1000 / 1000;
			Poco::Timespan span{fileReadDiff};

			log_debug(MinerLogger::plotReader, "%s done %s in %ss (~%.2hf GB/s)",
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

		log_information(MinerLogger::plotReader, "Dir %s done: %z files, %s total, %ss (~%.2hf GB/s)",
			plotReadNotification->dir,
			plotReadNotification->plotList.size(),
			memToString(totalSizeBytes, 2),
			Poco::DateTimeFormatter::format(span, "%s.%i"),
			totalSizeMBytes / dirReadDiffSeconds);
	}
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

	if (max_ > 0)
		MinerLogger::writeProgress(getProgress(), 48);
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

uintmax_t Burst::PlotReadProgress::getValue() const
{
	return progress_;
}

float Burst::PlotReadProgress::getProgress() const
{
	return progress_ * 1.f / max_ * 100;
}
