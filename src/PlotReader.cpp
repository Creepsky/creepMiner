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

Burst::PlotReader::PlotReader(Miner& miner, const std::string& path)
	: PlotReader(miner, std::make_unique<std::ifstream>(path, std::ifstream::binary),
		stoull(getAccountIdFromPlotFile(path)), stoull(getStartNonceFromPlotFile(path)),
		stoull(getNonceCountFromPlotFile(path)), stoull(getStaggerSizeFromPlotFile(path)))
{
	inputPath_ = path;
	//accountId_ = stoull(getAccountIdFromPlotFile(path));
	//nonceStart_ = stoull(getStartNonceFromPlotFile(path));
	//nonceCount_ = stoull(getNonceCountFromPlotFile(path));
	//staggerSize_ = stoull(getStaggerSizeFromPlotFile(path));
	//scoopNum_ = miner_.getScoopNum();
	//gensig_ = miner_.getGensig();
	////readBuffer_ = &buffer_[0];
	////writeBuffer_ = &buffer_[1];
}

Burst::PlotReader::PlotReader(Miner& miner, std::unique_ptr<std::istream> stream, size_t accountId, size_t nonceStart, size_t nonceCount, size_t staggerSize)
	: Poco::Task("PlotReader, startNonce: " + std::to_string(nonceStart)), inputStream_{std::move(stream)}, miner_{miner}
{
	inputPath_ = "";
	accountId_ = accountId;
	nonceStart_ = nonceStart;
	nonceCount_ = nonceCount;
	staggerSize_ = staggerSize;
	scoopNum_ = miner_.getScoopNum();
	gensig_ = miner_.getGensig();
}

void Burst::PlotReader::runTask()
{
	auto& inputStream = *inputStream_;

	if (inputStream.good())
	{
		std::vector<ScoopData> buffer;
		//Poco::ThreadPool threads{2, 64, 5};
		//Poco::TaskManager verifiers{threads};
		size_t chunkNum = 0;
		auto totalChunk = static_cast<size_t>(std::ceil(static_cast<double>(nonceCount_) / static_cast<double>(staggerSize_)));
		//std::deque<Task*> pendingVerifiers;

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
						buffer.resize(scoopBufferSize / Settings::ScoopSize);
				}

				if (scoopBufferSize > Settings::ScoopSize)
				{
					inputStream.seekg(startByte + staggerOffset);
					auto scoopData = reinterpret_cast<char*>(buffer.data());
					inputStream.read(scoopData, scoopBufferSize);
					
					//MinerLogger::write("chunk "+std::to_string(chunkNum)+" offset "+std::to_string(startByte + staggerOffset)+" read "+std::to_string(scoopBufferSize)+" nonce offset "+std::to_string(this->nonceOffset)+" nonceRead "+std::to_string(this->nonceRead));
					
					Shabal256 hash;
					
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

						auto nonceNum = nonceStart_ + nonceRead_ + i;
						miner_.submitNonce(nonceNum, accountId_, deadline, inputPath_);
						//++nonceRead_;
					}

					nonceRead_ += buffer.size();
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

		//inputStream_.close();
		//MinerLogger::write("finished reading plot file " + inputPath_);
		//MinerLogger::write("plot read done. "+Burst::getFileNameFromPath(this->inputPath)+" = "+std::to_string(this->nonceRead)+" nonces ");
	}
}

Burst::PlotListReader::PlotListReader(Miner& miner, std::shared_ptr<PlotReadProgress> progress,
	std::string&& dir, const std::vector<std::shared_ptr<PlotFile>>& plotFiles)
	: Task(dir),
	plotFileList_{&plotFiles}, miner_(&miner), progress_(progress), dir_{std::move(dir)}
{}

void Burst::PlotListReader::runTask()
{
	if (miner_ == nullptr)
		return;

	auto iter = plotFileList_->begin();

	while (iter != plotFileList_->end() && !isCancelled())
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

		if (progress_ != nullptr && !isCancelled())
		{
			progress_->add((*iter)->getSize());
			miner_->getData().addBlockEntry(createJsonProgress(progress_->getProgress()));
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
