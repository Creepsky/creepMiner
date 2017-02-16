#include "MinerData.hpp"
#include <Poco/ScopedLock.h>
#include "MinerConfig.hpp"
#include "MinerLogger.hpp"
#include "MinerShabal.hpp"
#include "Output.hpp"
#include "MinerUtil.hpp"
#include "Wallet.hpp"
#include "Account.hpp"

Burst::BlockData::BlockData(uint64_t blockHeight, uint64_t baseTarget, std::string genSigStr, MinerData* parent)
	: blockHeight_ {blockHeight},
	  baseTarget_ {baseTarget},
	  genSigStr_ {genSigStr},
	  activityLastWinner_ {this, &BlockData::runGetLastWinner},
	  parent_{parent}
{
	entries_ = std::make_shared<std::vector<Poco::JSON::Object>>();
	//deadlines_ = std::make_shared<std::unordered_map<AccountId, Deadlines>>();

	for (auto i = 0; i < 32; ++i)
	{
		auto byteStr = genSigStr.substr(i * 2, 2);
		genSig_[i] = static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16));
	}

	Shabal256 hash;
	GensigData newGenSig;

	hash.update(&genSig_[0], genSig_.size());
	hash.update(blockHeight);
	hash.close(&newGenSig[0]);

	scoop_ = (static_cast<int>(newGenSig[newGenSig.size() - 2] & 0x0F) << 8) | static_cast<int>(newGenSig[newGenSig.size() - 1]);
}

std::shared_ptr<Burst::Deadline> Burst::BlockData::addDeadline(uint64_t nonce, uint64_t deadline, std::shared_ptr<Account> account, uint64_t block, std::string plotFile)
{
	if (account == nullptr)
		return nullptr;

	Poco::ScopedLock<Poco::FastMutex> lock{mutex_};

	auto accountId = account->getId();

	auto iter = deadlines_.find(accountId);

	if (iter == deadlines_.end())
	{
		deadlines_.emplace(accountId, this);
		return deadlines_[accountId].add(nonce, deadline, account, block, plotFile);
	}

	return iter->second.add(nonce, deadline, account, block, plotFile);
}

void Burst::BlockData::setBaseTarget(uint64_t baseTarget)
{
	baseTarget_ = baseTarget;
}

void Burst::BlockData::confirmedDeadlineEvent(std::shared_ptr<Deadline> deadline)
{
	Poco::ScopedLock<Poco::FastMutex> lock{mutex_};

	if (deadline == nullptr)
		return;

	addBlockEntry(createJsonDeadline(*deadline, "deadline confirmed"));

	if (bestDeadline_ == nullptr ||
		bestDeadline_->getDeadline() > deadline->getDeadline())
		bestDeadline_ = deadline;

	if (parent_ != nullptr)
	{
		parent_->setBestDeadline(deadline);
		parent_->addConfirmedDeadline();
	}
}

void Burst::BlockData::setLastWinner(std::shared_ptr<Account> account)
{
	Poco::ScopedLock<Poco::FastMutex> lock{mutex_};
	lastWinner_ = account;

	auto lastWinnerJson = account->toJSON();
	lastWinnerJson->set("type", "lastWinner");
	addBlockEntry(*lastWinnerJson);
}

void Burst::BlockData::refreshBlockEntry() const
{
	Poco::ScopedLock<Poco::FastMutex> lock{mutex_};

	if (parent_ != nullptr)
		addBlockEntry(createJsonNewBlock(*parent_));
}

void Burst::BlockData::setProgress(float progress) const
{
	Poco::ScopedLock<Poco::FastMutex> lock{mutex_};
	addBlockEntry(createJsonProgress(progress));
}

void Burst::BlockData::addBlockEntry(Poco::JSON::Object entry) const
{
	Poco::ScopedLock<Poco::FastMutex> lock{mutex_};

	entries_->emplace_back(std::move(entry));
	
	if (parent_ != nullptr)
		parent_->notifiyBlockDataChanged_.postNotification(new BlockDataChangedNotification{&entry});
}

uint64_t Burst::BlockData::getBlockheight() const
{
	return blockHeight_.load();
}

uint64_t Burst::BlockData::getScoop() const
{
	return scoop_.load();
}

uint64_t Burst::BlockData::getBasetarget() const
{
	return baseTarget_.load();
}

std::shared_ptr<Burst::Account> Burst::BlockData::getLastWinner() const
{
	Poco::ScopedLock<Poco::FastMutex> lock{mutex_};
	return lastWinner_;
}
 
Poco::ActiveResult<std::shared_ptr<Burst::Account>> Burst::BlockData::getLastWinnerAsync(const Wallet& wallet, const Accounts& accounts)
{
	Poco::ScopedLock<Poco::FastMutex> lock{mutex_};
	auto tuple = std::make_tuple(&wallet, &accounts);
	return activityLastWinner_(tuple);
}

const Burst::GensigData& Burst::BlockData::getGensig() const
{
	Poco::ScopedLock<Poco::FastMutex> lock{mutex_};
	return genSig_;
}

const std::string& Burst::BlockData::getGensigStr() const
{
	Poco::ScopedLock<Poco::FastMutex> lock{mutex_};
	return genSigStr_;
}

std::shared_ptr<Burst::Deadline> Burst::BlockData::getBestDeadline() const
{
	Poco::ScopedLock<Poco::FastMutex> lock{mutex_};
	return bestDeadline_;
}

std::vector<Poco::JSON::Object> Burst::BlockData::getEntries() const
{
	Poco::ScopedLock<Poco::FastMutex> lock{mutex_};

	return *entries_;
}

const std::unordered_map<Burst::AccountId, Burst::Deadlines>& Burst::BlockData::getDeadlines() const
{
	Poco::ScopedLock<Poco::FastMutex> lock{mutex_};
	return deadlines_;
}

std::shared_ptr<Burst::Deadline> Burst::BlockData::getBestDeadline(uint64_t accountId, BlockData::DeadlineSearchType searchType)
{
	Poco::ScopedLock<Poco::FastMutex> lock {mutex_};

	auto iter = deadlines_.find(accountId);

	if (iter == deadlines_.end())
		return nullptr;

	switch (searchType)
	{
		case DeadlineSearchType::Found:
			return iter->second.getBestFound();
		case DeadlineSearchType::Sent:
			return iter->second.getBestSent();
		case DeadlineSearchType::Confirmed:
			return iter->second.getBestConfirmed();
		default:
			return nullptr;
	}
}

std::shared_ptr<Burst::Account> Burst::BlockData::runGetLastWinner(const std::tuple<const Wallet*, const Accounts*>& args)
{
	poco_ndc(Miner::runGetLastWinner);

	AccountId lastWinner;
	auto lastBlockheight = blockHeight_ - 1;
	auto& wallet = *std::get<0>(args);
	auto& accounts = *std::get<1>(args);

	// we cheat here a little bit and use the submission max retry set in config
	// for max retry fetching the last winner
	for (auto loop = 0u;
		loop < MinerConfig::getConfig().getSubmissionMaxRetry() ||
		MinerConfig::getConfig().getSubmissionMaxRetry() == 0;
		++loop)
	{
		if (wallet.getWinnerOfBlock(lastBlockheight, lastWinner))
		{
			// we are the winner :)
			if (accounts.isLoaded(lastWinner))
			{
				parent_->addWonBlock();
				// we (re)send the good news to the local html server
				addBlockEntry(createJsonNewBlock(*parent_));
			}

			auto winnerAccount = std::make_shared<Account>(wallet, lastWinner);
			winnerAccount->getName();

			// TODO REWORK
			log_ok_if(MinerLogger::miner, MinerLogger::hasOutput(LastWinner), std::string(50, '-') + "\n"
				"last block winner: \n"
				"block#             %Lu\n"
				"winner-numeric     %Lu\n"
				"winner-address     %s\n"
				"%s" +
				std::string(50, '-'),
				lastBlockheight, lastWinner, winnerAccount->getAddress(),
				(winnerAccount->getName().empty() ? "" : Poco::format("winner-name        %s\n", winnerAccount->getName()))
			);

			setLastWinner(winnerAccount);

			return winnerAccount;
		}
	}

	return nullptr;
}

std::shared_ptr<Burst::BlockData> Burst::MinerData::startNewBlock(uint64_t block, uint64_t baseTarget, const std::string& genSig)
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

	blockData_ = std::make_shared<BlockData>(block, baseTarget, genSig, this);

	return blockData_;
}

void Burst::MinerData::addConfirmedDeadline()
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};
	++deadlinesConfirmed_;
}

void Burst::MinerData::setBestDeadline(std::shared_ptr<Deadline> deadline)
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};

	// we check if its the best deadline overall
	if (bestDeadlineOverall_ == nullptr ||
		deadline->getDeadline() < bestDeadlineOverall_->getDeadline())
		bestDeadlineOverall_ = deadline;
}

void Burst::MinerData::setTargetDeadline(uint64_t deadline)
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};
	targetDeadline_ = deadline;
}

void Burst::MinerData::addWonBlock()
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};
	++blocksWon_;
}

std::shared_ptr<Burst::Deadline> Burst::MinerData::getBestDeadlineOverall() const
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

//std::shared_ptr<const Poco::JSON::Object> Burst::MinerData::getLastWinner() const
//{
//	Poco::ScopedLock<Poco::Mutex> lock{mutex_};
//
//	// we make a local copy here, because the block data could be changed
//	// every second and we would have undefined behaviour
//	auto blockData = getBlockData();
//
//	if (blockData == nullptr)
//		return nullptr;
//
//	return blockData->getLastWinner();
//}

std::shared_ptr<const Burst::BlockData> Burst::MinerData::getHistoricalBlockData(uint32_t roundsBefore) const
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};

	if (roundsBefore == 0)
		return getBlockData();

	auto wantedBlock = getBlockData()->getBlockheight() - roundsBefore;

	auto iter = std::find_if(historicalBlocks_.begin(), historicalBlocks_.end(), [wantedBlock](const std::shared_ptr<BlockData>& data)
	{
		return data->getBlockheight() == wantedBlock;
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

	auto targetDeadline = targetDeadline_;
	auto manualTargetDeadline = MinerConfig::getConfig().getTargetDeadline();

	if (targetDeadline == 0)
		targetDeadline = manualTargetDeadline;
	else if (targetDeadline > manualTargetDeadline &&
		manualTargetDeadline > 0)
		targetDeadline = manualTargetDeadline;

	return targetDeadline;
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
		auto bestDeadline = block->getBestDeadline();

		if (bestDeadline != nullptr)
			avg += bestDeadline->getDeadline();
	});

	return avg / historicalBlocks_.size();
}
