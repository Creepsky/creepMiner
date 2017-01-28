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

Burst::PlotReader::PlotReader(Miner& miner, std::shared_ptr<Burst::PlotReadProgress> progress,
std::string dir, const std::vector<std::shared_ptr< Burst::PlotFile>>& plotList, Poco::NotificationQueue& queue)
	: Task("PlotReader"), miner_{miner}, progress_{progress}, dir_{std::move(dir)}, queue_{&queue}
{
	scoopNum_ = miner_.getScoopNum();
	gensig_ = miner_.getGensig();
	plotList_ = &plotList;

}

void Burst::PlotReader::runTask()
{
	for (auto plotFileIter = plotList_->begin(); plotFileIter != plotList_->end() && !isCancelled(); ++plotFileIter)
	{
		auto& plotFile = *(*plotFileIter);

		std::ifstream inputStream(plotFile.getPath(), std::ifstream::binary);

		if (inputStream.good())
		{
			auto accountId = stoull(getAccountIdFromPlotFile(plotFile.getPath()));
			auto nonceStart = stoull(getStartNonceFromPlotFile(plotFile.getPath()));
			auto nonceCount = stoull(getNonceCountFromPlotFile(plotFile.getPath()));
			auto staggerSize = stoull(getStaggerSizeFromPlotFile(plotFile.getPath()));
			auto block = miner_.getBlockheight();
			nonceRead_ = 0;

			size_t chunkNum = 0;
			auto totalChunk = static_cast<size_t>(std::ceil(static_cast<double>(nonceCount) / static_cast<double>(staggerSize)));

			//MinerLogger::write("reading plot file " + plotFile.getPath());

			while (!isCancelled() && inputStream.good() && chunkNum <= totalChunk)
			{
				// setting up plot data
				VerifyNotification::Ptr verification = new VerifyNotification{};
				verification->accountId = accountId;
				verification->nonceStart = nonceStart;
				verification->block = block;
				verification->inputPath = plotFile.getPath();

				auto scoopBufferSize = MinerConfig::getConfig().maxBufferSizeMB * 1024 * 1024 / (64 * 2); // 8192
				auto scoopBufferCount = static_cast<size_t>(
					std::ceil(static_cast<float>(staggerSize * Settings::ScoopSize) / static_cast<float>(scoopBufferSize)));
				auto startByte = scoopNum_ * Settings::ScoopSize * staggerSize + chunkNum * staggerSize * Settings::PlotSize;
				auto scoopDoneRead = 0u;
				size_t staggerOffset;

				while (!isCancelled() && inputStream.good() && scoopDoneRead <= scoopBufferCount)
				{
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
						verification->nonceRead = nonceRead_;

						//MinerLogger::write("chunk "+std::to_string(chunkNum)+" offset "+std::to_string(startByte + staggerOffset)+" read "+std::to_string(scoopBufferSize)+" nonce offset "+std::to_string(this->nonceOffset)+" nonceRead "+std::to_string(this->nonceRead));

						// wait, when there is too much work for the verifiers
						while (!isCancelled() && queue_->size() >= MinerConfig::getConfig().getMiningIntensity() * 2)
						{ }

						queue_->enqueueNotification(verification);

						nonceRead_ += bufferSize;
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
	}

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
		MinerLogger::writeProgress(getProgress(), 48);
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

uintmax_t Burst::PlotReadProgress::getValue() const
{
	return progress_;
}

float Burst::PlotReadProgress::getProgress() const
{
	return progress_ * 1.f / max_ * 100;
}
