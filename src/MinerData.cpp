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

	refreshBlockEntry();
}

std::shared_ptr<Burst::Deadline> Burst::BlockData::addDeadline(uint64_t nonce, uint64_t deadline, std::shared_ptr<Account> account, uint64_t block, std::string plotFile)
{
	if (account == nullptr)
		return nullptr;

	auto accountId = account->getId();

	Poco::ScopedLock<Poco::Mutex> lock{mutex_};
	
	auto iter = deadlines_.find(accountId);

	if (iter == deadlines_.end())
	{
		deadlines_.insert(std::make_pair(accountId, Deadlines(this)));
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
	if (deadline == nullptr)
		return;

	addBlockEntry(createJsonDeadline(*deadline, "nonce confirmed"));

	// set the best deadline for this block
	{
		Poco::ScopedLock<Poco::Mutex> lock{mutex_};

		if (bestDeadline_ == nullptr ||
			bestDeadline_->getDeadline() > deadline->getDeadline())
			bestDeadline_ = deadline;
	}

	// set the best deadline overall
	if (parent_ != nullptr)
	{
		parent_->setBestDeadline(deadline);
		parent_->addConfirmedDeadline();
	}
}

void Burst::BlockData::setLastWinner(std::shared_ptr<Account> account)
{
	// set the winner for the last block
	{
		Poco::ScopedLock<Poco::Mutex> lock{mutex_};
		lastWinner_ = account;
	}

	auto lastWinnerJson = account->toJSON();
	lastWinnerJson->set("type", "lastWinner");
	addBlockEntry(*lastWinnerJson);
}

void Burst::BlockData::refreshBlockEntry() const
{
	addBlockEntry(createJsonNewBlock(*parent_));
}

void Burst::BlockData::setProgress(float progress)
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};

	jsonProgress_ = new Poco::JSON::Object{createJsonProgress(progress)};

	if (parent_ != nullptr)
		parent_->blockDataChangedEvent.notifyAsync(this, *jsonProgress_);
}

void Burst::BlockData::addBlockEntry(Poco::JSON::Object entry) const
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};

	entries_->emplace_back(std::move(entry));
	
	if (parent_ != nullptr)
		parent_->blockDataChangedEvent.notifyAsync(this, entry);
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
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};
	return lastWinner_;
}
 
Poco::ActiveResult<std::shared_ptr<Burst::Account>> Burst::BlockData::getLastWinnerAsync(const Wallet& wallet, const Accounts& accounts)
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};
	auto tuple = std::make_pair(&wallet, &accounts);
	return activityLastWinner_(tuple);
}

const Burst::GensigData& Burst::BlockData::getGensig() const
{
	return genSig_;
}

const std::string& Burst::BlockData::getGensigStr() const
{
	return genSigStr_;
}

std::shared_ptr<Burst::Deadline> Burst::BlockData::getBestDeadline() const
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};
	return bestDeadline_;
}

bool Burst::BlockData::forEntries(std::function<bool(const Poco::JSON::Object&)> traverseFunction) const
{
	Poco::ScopedLock<Poco::Mutex> lock{mutex_};

	if (entries_ == nullptr)
		return false;

	auto error = false;

	for (auto iter = entries_->begin(); iter != entries_->end() && !error; ++iter)
		error = !traverseFunction(*iter);

	// send the progress, if any
	if (!error && !jsonProgress_.isNull())
		error = !traverseFunction(*jsonProgress_);

	return error;
}

//std::vector<Poco::JSON::Object> Burst::BlockData::getEntries() const
//{
//	Poco::ScopedLock<Poco::Mutex> lock{mutex_};
//	return *entries_;
//}

//const std::unordered_map<Burst::AccountId, Burst::Deadlines>& Burst::BlockData::getDeadlines() const
//{
//	Poco::ScopedLock<Poco::FastMutex> lock{mutex_};
//	return deadlines_;
//}

std::shared_ptr<Burst::Deadline> Burst::BlockData::getBestDeadline(uint64_t accountId, BlockData::DeadlineSearchType searchType)
{
	Poco::ScopedLock<Poco::Mutex> lock {mutex_};

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

std::shared_ptr<Burst::Deadline> Burst::BlockData::addDeadlineIfBest(uint64_t nonce, uint64_t deadline, std::shared_ptr<Account> account, uint64_t block, std::string plotFile)
{
	Poco::ScopedLock<Poco::Mutex> lock {mutex_};

	auto bestDeadline = this->getBestDeadline(account->getId(), BlockData::DeadlineSearchType::Found);

	if (bestDeadline == nullptr || bestDeadline->getDeadline() > deadline)
		return addDeadline(nonce, deadline, account, block, plotFile);

	return nullptr;
}

void Burst::BlockData::addMessage(const Poco::Message& message) const
{
	Poco::ScopedLock<Poco::Mutex> lock {mutex_};

	if (entries_ == nullptr)
		return;

	Poco::JSON::Object json;
	json.set("type", std::to_string(static_cast<int>(message.getPriority())));
	json.set("text", message.getText());
	json.set("source", message.getSource());
	json.set("line", message.getSourceLine());
	json.set("file", message.getSourceFile());
	json.set("time", Poco::DateTimeFormatter::format(message.getTime(), "%H:%M:%S"));

	entries_->emplace_back(json);

	if (parent_ != nullptr)
		parent_->blockDataChangedEvent.notifyAsync(this, json);
}

void Burst::BlockData::clearEntries() const
{
	Poco::ScopedLock<Poco::Mutex> lock {mutex_};
	entries_->clear();
}

std::shared_ptr<Burst::Account> Burst::BlockData::runGetLastWinner(const std::pair<const Wallet*, const Accounts*>& args)
{
	poco_ndc(Miner::runGetLastWinner);

	AccountId lastWinner;
	auto lastBlockheight = blockHeight_ - 1;
	auto& wallet = *args.first;
	auto& accounts = *args.second;

	if (!wallet.isActive())
		return nullptr;

	// TODO: would be good to have a setting for this 
	const auto maxLoops = 5;

	for (auto loop = 0; loop < maxLoops; ++loop)
	{
		log_debug(MinerLogger::wallet, "get last winner loop %i/%i", loop + 1, maxLoops);

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
			auto futureName = winnerAccount->getNameAsync();
			futureName.wait();

			log_ok_if(MinerLogger::miner, MinerLogger::hasOutput(LastWinner), std::string(50, '-') + "\n"
				"last block winner: \n"
				"block#             %Lu\n"
				"winner-numeric     %Lu\n"
				"winner-address     %s\n"
				"%s" +
				std::string(50, '-'),
				lastBlockheight, lastWinner, winnerAccount->getAddress(),
				(winnerAccount->getName().empty() ? "" : Poco::format("winner-name        %s\n", winnerAccount->getName()))
			)
;

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

		// we clear all entries, because it is also in the logfile
		// and we dont need it in the ram anymore
		blockData_->clearEntries();

		historicalBlocks_.emplace_back(blockData_);

		++blocksMined_;
	}

	blockData_ = std::make_shared<BlockData>(block, baseTarget, genSig, this);
	
	currentBlockheight_ = block;
	currentBasetarget_ = baseTarget;
	currentScoopNum_ = blockData_->getScoop();

	return blockData_;
}

void Burst::MinerData::addConfirmedDeadline()
{
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
	targetDeadline_ = deadline;
}

void Burst::MinerData::addMessage(const Poco::Message& message)
{
	auto blockData = getBlockData();

	if (blockData != nullptr)
		blockData->addMessage(message);
}

void Burst::MinerData::addWonBlock()
{
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
	return blocksMined_;
}

uint64_t Burst::MinerData::getBlocksWon() const
{
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
	if (roundsBefore == 0)
		return getBlockData();

	auto wantedBlock = getBlockData()->getBlockheight() - roundsBefore;

	Poco::ScopedLock<Poco::Mutex> lock{mutex_};

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
	return deadlinesConfirmed_;
}

uint64_t Burst::MinerData::getTargetDeadline() const
{
	auto targetDeadline = targetDeadline_.load();
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

uint64_t Burst::MinerData::getCurrentBlockheight() const
{
	return currentBlockheight_.load();
}

uint64_t Burst::MinerData::getCurrentBasetarget() const
{
	return currentBasetarget_.load();
}

uint64_t Burst::MinerData::getCurrentScoopNum() const
{
	return currentScoopNum_.load();
}
