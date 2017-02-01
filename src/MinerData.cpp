#include "MinerData.hpp"
#include <Poco/ScopedLock.h>
#include "MinerConfig.hpp"
#include "MinerLogger.hpp"

void Burst::MinerData::setBestDeadline(std::shared_ptr<Deadline> deadline)
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};

	auto data = getBlockData();

	// first we check if the confirmed deadline is better then the current best
	if (data != nullptr)
		if (data->bestDeadline == nullptr ||
				data->bestDeadline->getDeadline() > deadline->getDeadline())
			data->bestDeadline = deadline;

	// then we check if its the best deadline overall
	if (bestDeadlineOverall_ == nullptr ||
		deadline->getDeadline() < bestDeadlineOverall_->getDeadline())
		bestDeadlineOverall_ = deadline;	
}

void Burst::MinerData::startNewBlock(uint64_t block, uint64_t scoop, uint64_t baseTarget, const std::string& genSig)
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};

	// save the old data in the historical container
	if (blockData_ != nullptr)
	{
		const auto maxSize = 30;

		// if we reached the maximum size of blocks, forget the oldest
		if (historicalBlocks_.size() + 1 > maxSize)
			historicalBlocks_.pop_front();

		historicalBlocks_.emplace_back(blockData_);

		++blocksMined_;
	}

	blockData_ = std::make_shared<BlockData>();
	blockData_->block = block;
	blockData_->baseTarget = baseTarget;
	blockData_->genSig = genSig;
	blockData_->scoop = scoop;
}

void Burst::MinerData::addBlockEntry(Poco::JSON::Object entry)
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};
	auto blockData = blockData_;

	if (blockData != nullptr)
	{
		blockData->entries.emplace_back(std::move(entry));
		notifiyBlockDataChanged_.postNotification(new BlockDataChangedNotification{&entry});
	}
}

void Burst::MinerData::setLastWinner(std::shared_ptr<Poco::JSON::Object> winner) const
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};
	auto blockData = blockData_;

	if (blockData != nullptr)
		blockData->lastWinner = winner;
}

void Burst::MinerData::addConfirmedDeadline()
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};
	++deadlinesConfirmed_;
}

void Burst::MinerData::setTargetDeadline(uint64_t deadline)
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};
	//MinerConfig::getConfig().get
	targetDeadline_ = deadline;
}

void Burst::MinerData::setBestDeadlineCurrent(std::shared_ptr<Deadline> deadline)
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};
	auto block = getBlockData();
	
	if (block == nullptr || deadline == nullptr)
		return;

	if (block->bestDeadline == nullptr ||
		block->bestDeadline->getDeadline() > deadline->getDeadline())
	{
		block->bestDeadline = deadline;
	}
}

void Burst::MinerData::addWonBlock()
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};
	++blocksWon_;
}

std::shared_ptr<const Burst::Deadline> Burst::MinerData::getBestDeadlineOverall() const
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};
	return bestDeadlineOverall_;
}

const Poco::Timestamp& Burst::MinerData::getStartTime() const
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};
	return startTime_;
}

Poco::Timespan Burst::MinerData::getRunTime() const
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};
	return Poco::Timestamp{} - getStartTime();
}

uint64_t Burst::MinerData::getCurrentBlock() const
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};
	
	if (blockData_ == nullptr)
		return 0;

	return blockData_->block;
}

uint64_t Burst::MinerData::getBlocksMined() const
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};
	return blocksMined_;
}

uint64_t Burst::MinerData::getBlocksWon() const
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};
	return blocksWon_;
}

std::shared_ptr<Burst::BlockData> Burst::MinerData::getBlockData()
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};
	return blockData_;
}

std::shared_ptr<const Burst::BlockData> Burst::MinerData::getBlockData() const
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};
	return blockData_;
}

std::shared_ptr<const Poco::JSON::Object> Burst::MinerData::getLastWinner() const
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};

	// we make a local copy here, because the block data could be changed
	// every second and we would have undefined behaviour
	auto blockData = getBlockData();

	if (blockData == nullptr)
		return nullptr;

	return blockData->lastWinner;
}

std::shared_ptr<const Burst::BlockData> Burst::MinerData::getHistoricalBlockData(uint32_t roundsBefore) const
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};

	if (roundsBefore == 0)
		return getBlockData();

	auto wantedBlock = getCurrentBlock() - roundsBefore;

	auto iter = std::find_if(historicalBlocks_.begin(), historicalBlocks_.end(), [wantedBlock](const std::shared_ptr<BlockData>& data)
	{
		return data->block == wantedBlock;
	});

	if (iter == historicalBlocks_.end())
		return nullptr;

	return (*iter);
}

std::vector<std::shared_ptr<const Burst::BlockData>> Burst::MinerData::getAllHistoricalBlockData() const
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};
	
	std::vector<std::shared_ptr<const BlockData>> blocks;
	blocks.reserve(historicalBlocks_.size());

	for (auto& block : historicalBlocks_)
		blocks.emplace_back(block);

	return blocks;
}

uint64_t Burst::MinerData::getConfirmedDeadlines() const
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};
	return deadlinesConfirmed_;
}

uint64_t Burst::MinerData::getTargetDeadline() const
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};
	return targetDeadline_;
}

bool Burst::MinerData::compareToTargetDeadline(uint64_t deadline) const
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};
	return deadline < getTargetDeadline();
}

uint64_t Burst::MinerData::getAverageDeadline() const
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};
	uint64_t avg = 0;

	if (historicalBlocks_.empty())
		return 0;

	std::for_each(historicalBlocks_.begin(), historicalBlocks_.end(), [&avg](const std::shared_ptr<const BlockData>& block)
	{
		if (block->bestDeadline != nullptr)
			avg += block->bestDeadline->getDeadline();
	});

	return avg / historicalBlocks_.size();
}
