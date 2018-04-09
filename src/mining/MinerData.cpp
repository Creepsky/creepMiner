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

#include "MinerData.hpp"
#include "MinerConfig.hpp"
#include "logging/MinerLogger.hpp"
#include "shabal/MinerShabal.hpp"
#include "logging/Output.hpp"
#include "MinerUtil.hpp"
#include "wallet/Wallet.hpp"
#include "wallet/Account.hpp"
#include <Poco/Data/Session.h>

using namespace Poco::Data::Keywords;

Burst::BlockData::BlockData(const Poco::UInt64 blockHeight, const Poco::UInt64 baseTarget, const std::string& genSigStr,
                            MinerData* parent, const Poco::UInt64 blockTargetDeadline)
	: blockHeight_ {blockHeight},
	  baseTarget_ {baseTarget},
	  blockTargetDeadline_{ blockTargetDeadline },
	  genSigStr_ {genSigStr},
	  parent_{parent}
{
	entries_ = std::make_shared<std::vector<Poco::JSON::Object>>();
	//deadlines_ = std::make_shared<std::unordered_map<AccountId, Deadlines>>();

	for (auto i = 0; i < 32; ++i)
	{
		const auto byteStr = genSigStr.substr(i * 2, 2);
		genSig_[i] = static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16));
	}

	Shabal256_SSE2 hash;
	GensigData newGenSig;
	
	hash.update(&genSig_[0], genSig_.size());
	hash.update(blockHeight);
	hash.close(&newGenSig[0]);

	roundTime_ = 0;
	scoop_ = (static_cast<int>(newGenSig[newGenSig.size() - 2] & 0x0F) << 8) | static_cast<int>(newGenSig[newGenSig.size() - 1]);

	refreshBlockEntry();
}

std::shared_ptr<Burst::Deadline> Burst::BlockData::addDeadlineUnlocked(const Poco::UInt64 nonce, const Poco::UInt64 deadline,
                                                                       const std::shared_ptr<Account>& account, const Poco::UInt64 block,
                                                                       const std::string& plotFile)
{
	if (account == nullptr)
		return nullptr;

	auto accountId = account->getId();
	
	auto iter = deadlines_.find(accountId);

	if (iter == deadlines_.end())
	{
		deadlines_.insert(std::make_pair(accountId, std::make_shared<Deadlines>(this)));
		return deadlines_[accountId]->add(nonce, deadline, account, block, plotFile);
	}

	return iter->second->add(nonce, deadline, account, block, plotFile);
}

std::shared_ptr<Burst::Deadline> Burst::BlockData::addDeadline(const Poco::UInt64 nonce, const Poco::UInt64 deadline,
                                                               const std::shared_ptr<Account>& account,
                                                               const Poco::UInt64 block, const std::string& plotFile)
{
	std::lock_guard<std::mutex> lock{ mutex_ };
	return addDeadlineUnlocked(nonce, deadline, account, block, plotFile);
}

void Burst::BlockData::setBaseTarget(Poco::UInt64 baseTarget)
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
		std::lock_guard<std::mutex> lock{ mutex_ };

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

Burst::BlockData::DataLoader::DataLoader()
	: getLastWinner{this, &DataLoader::runGetLastWinner}
{}

Burst::BlockData::DataLoader::~DataLoader() = default;

Burst::BlockData::DataLoader& Burst::BlockData::DataLoader::getInstance()
{
	static DataLoader data_loader;
	return data_loader;
}

std::shared_ptr<Burst::Account> Burst::BlockData::DataLoader::runGetLastWinner(const std::tuple<const Wallet&, Accounts&, BlockData&>& args)
{
	auto& wallet = std::get<0>(args);
	auto& accounts = std::get<1>(args);
	auto& blockdata = std::get<2>(args);
	
	AccountId lastWinner;
	const auto lastBlockheight = blockdata.blockHeight_ - 1;
	
	if (!wallet.isActive())
		return nullptr;

	if (wallet.getWinnerOfBlock(lastBlockheight, lastWinner))
	{
		auto winnerAccount = accounts.getAccount(lastWinner, wallet, false);

		winnerAccount->getOrLoadName().wait();
		winnerAccount->getOrLoadRewardRecipient().wait();

		std::string rewardRecipient;

		if (winnerAccount->getRewardRecipient() == winnerAccount->getId())
			rewardRecipient = "                   Solo mining";
		else
		{
			auto rewardRecipientAccount = accounts.getAccount(winnerAccount->getRewardRecipient(), wallet, false);
			rewardRecipientAccount->getOrLoadName().wait();
			rewardRecipient = "Pool               " + rewardRecipientAccount->getName();
		}

		log_ok_if(MinerLogger::miner, MinerLogger::hasOutput(LastWinner), std::string(50, '-') + "\n"
			"last block winner: \n"
			"block#             %s\n"
			"winner-numeric     %Lu\n"
			"winner-address     %s\n"
			"%s" +
			"%s\n" +
			std::string(50, '-'),
			numberToString(lastBlockheight), lastWinner, winnerAccount->getAddress(),
			winnerAccount->getName().empty() ? "" : Poco::format("winner-name        %s\n", winnerAccount->getName()),
			rewardRecipient
		)

;

		blockdata.setLastWinner(winnerAccount);

		return winnerAccount;
	}

	return nullptr;
}

void Burst::BlockData::setLastWinner(std::shared_ptr<Account> account)
{
	// set the winner for the last block
	{
		std::lock_guard<std::mutex> lock{mutex_};
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

void Burst::BlockData::refreshConfig() const
{
	addBlockEntry(createJsonConfig());
}

void Burst::BlockData::refreshPlotDirs() const
{
	addBlockEntry(createJsonPlotDirsRescan());
}

void Burst::BlockData::setProgress(float progressRead, float progressVerification, Poco::UInt64 blockheight)
{
	if (blockheight != getBlockheight())
		return;

	{
		std::lock_guard<std::mutex> lock{ mutex_ };
		jsonProgress_ = new Poco::JSON::Object{createJsonProgress(progressRead, progressVerification)};
	}

	if (parent_ != nullptr)
		parent_->blockDataChangedEvent.notify(this, *jsonProgress_);
}

void Burst::BlockData::setProgress(const std::string& plotDir, float progress, Poco::UInt64 blockheight)
{
	if (blockheight != getBlockheight())
		return;

	std::lock_guard<std::mutex> lock{ mutex_ };
	auto json = new Poco::JSON::Object{ createJsonProgress(progress, 0.f) };
	json->set("type", "plotdir-progress");
	json->set("dir", plotDir);

	jsonDirProgress_[plotDir].assign(json);

	if (parent_ != nullptr)
		parent_->blockDataChangedEvent.notify(this, *json);
}

void Burst::BlockData::setRoundTime(double rTime)
{
	roundTime_ = rTime;
}

void Burst::BlockData::addBlockEntry(Poco::JSON::Object entry) const
{
	{
		std::lock_guard<std::mutex> lock{ mutex_ };
		entries_->emplace_back(entry);	
	}
	
	if (parent_ != nullptr)
		parent_->blockDataChangedEvent.notify(this, entry);
}

Poco::UInt64 Burst::BlockData::getBlockheight() const
{
	return blockHeight_.load();
}

Poco::UInt64 Burst::BlockData::getScoop() const
{
	return scoop_.load();
}

Poco::UInt64 Burst::BlockData::getBasetarget() const
{
	return baseTarget_.load();
}

Poco::UInt64 Burst::BlockData::getDifficulty() const
{
	return 18325193796 / getBasetarget();
}

double Burst::BlockData::getRoundTime() const
{
	return roundTime_;
}

Poco::UInt64 Burst::BlockData::getBlockTargetDeadline() const
{
	return blockTargetDeadline_.load();
}

std::shared_ptr<Burst::Account> Burst::BlockData::getLastWinner() const
{
	return lastWinner_;
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
	return bestDeadline_;
}

std::shared_ptr<Burst::Deadline> Burst::BlockData::getBestDeadline(const DeadlineSearchType searchType) const
{
	std::lock_guard<std::mutex> lock{ mutex_ };
	std::shared_ptr<Deadline> bestDeadline;

	for (const auto& accountDeadlines : deadlines_)
	{
		if (accountDeadlines.second == nullptr)
			continue;
	
		std::shared_ptr<Deadline> accountBestDeadline;

		if (searchType == DeadlineSearchType::Found)
			accountBestDeadline = accountDeadlines.second->getBestFound();
		else if (searchType == DeadlineSearchType::Sent)
			accountBestDeadline = accountDeadlines.second->getBestSent();
		else if (searchType == DeadlineSearchType::Confirmed)
			accountBestDeadline = accountDeadlines.second->getBestConfirmed();

		if (accountBestDeadline == nullptr)
			continue;

		if (bestDeadline == nullptr ||
			bestDeadline->getDeadline() > accountBestDeadline->getDeadline())
			bestDeadline = accountBestDeadline;
	}

	return bestDeadline;
}

bool Burst::BlockData::forEntries(std::function<bool(const Poco::JSON::Object&)> traverseFunction) const
{
	std::lock_guard<std::mutex> lock{mutex_};

	if (entries_ == nullptr)
		return false;

	auto error = false;

	for (auto iter = entries_->begin(); iter != entries_->end() && !error; ++iter)
		error = !traverseFunction(*iter);

	// send the overall progress, if any
	if (!error && !jsonProgress_.isNull())
		error = !traverseFunction(*jsonProgress_);

	// send the dir progress
	if (!error)
		for (auto iter = jsonDirProgress_.begin(); !error && iter != jsonDirProgress_.end(); ++iter)
			error = !traverseFunction(*iter->second);

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

std::shared_ptr<Burst::Deadline> Burst::BlockData::getBestDeadlineUnlocked(Poco::UInt64 accountId, Burst::BlockData::DeadlineSearchType searchType)
{
	const auto iter = deadlines_.find(accountId);

	if (iter == deadlines_.end())
		return nullptr;

	switch (searchType)
	{
	case DeadlineSearchType::Found:
		return iter->second->getBestFound();
	case DeadlineSearchType::Sent:
		return iter->second->getBestSent();
	case DeadlineSearchType::Confirmed:
		return iter->second->getBestConfirmed();
	default:
		return nullptr;
	}
}

std::shared_ptr<Burst::Deadline> Burst::BlockData::getBestDeadline(Poco::UInt64 accountId, BlockData::DeadlineSearchType searchType)
{
	std::lock_guard<std::mutex> lock{ mutex_ };
	return getBestDeadlineUnlocked(accountId, searchType);
}

Poco::ActiveResult<std::shared_ptr<Burst::Account>> Burst::BlockData::getLastWinnerAsync(const Wallet& wallet, Accounts& accounts)
{
	return DataLoader::getInstance().getLastWinner(make_tuple(std::cref(wallet), std::ref(accounts), std::ref(*this)));
}

std::shared_ptr<Burst::Deadline> Burst::BlockData::addDeadlineIfBest(const Poco::UInt64 nonce,
                                                                     const Poco::UInt64 deadline,
                                                                     const std::shared_ptr<Account>& account,
                                                                     const Poco::UInt64 block,
                                                                     const std::string& plotFile)
{
	std::lock_guard<std::mutex> lock{ mutex_ };

	const auto bestDeadline = getBestDeadlineUnlocked(account->getId(), DeadlineSearchType::Found);

	if (bestDeadline == nullptr || bestDeadline->getDeadline() > deadline)
		return addDeadlineUnlocked(nonce, deadline, account, block, plotFile);

	return nullptr;
}

void Burst::BlockData::addMessage(const Poco::Message& message) const
{
	Poco::JSON::Object json;

	{
		std::lock_guard<std::mutex> lock{ mutex_ };

		if (entries_ == nullptr)
			return;
		
		json.set("type", std::to_string(static_cast<int>(message.getPriority())));
		json.set("text", message.getText());
		json.set("source", message.getSource());
		json.set("line", message.getSourceLine());
		json.set("file", message.getSourceFile());
		json.set("time", Poco::DateTimeFormatter::format(Poco::LocalDateTime(message.getTime()), "%H:%M:%S"));

		entries_->emplace_back(json);
	}

	if (parent_ != nullptr)
		parent_->blockDataChangedEvent.notify(this, json);
}

void Burst::BlockData::clearEntries() const
{
	std::lock_guard<std::mutex> lock{ mutex_ };
	entries_->clear();
}

bool Burst::BlockData::forDeadlines(const std::function<bool(const Deadline&)>& traverseFunction) const
{
	std::lock_guard<std::mutex> lock{mutex_};

	if (deadlines_.empty())
		return false;

	auto error = false;

	for (auto iterAccounts = deadlines_.begin(); iterAccounts != deadlines_.end() && !error; ++iterAccounts)
	{
		if (iterAccounts->second == nullptr)
			continue;

		const auto deadlines = iterAccounts->second->getDeadlines();

		for (auto iter = deadlines.begin(); iter != deadlines.end() && !error; ++iter)
			error = !traverseFunction(**iter);
	}

	return error;
}

Burst::MinerData::MinerData()
	: lowestDifficulty_{0, 0},
	  highestDifficulty_ {0, 0},
	  blocksMined_(0),
	  blocksWon_(0),
	  deadlinesConfirmed_(0),
	  currentBlockheight_(0),
	  currentBasetarget_(0),
	  currentScoopNum_(0),
	  activityWonBlocks_{this, &MinerData::runGetWonBlocks}
{
	const auto databasePath = MinerConfig::getConfig().getDatabasePath();

	try
	{
		dbSession_ = std::make_unique<Poco::Data::Session>("SQLite", databasePath);
		
		*dbSession_ <<
			"CREATE TABLE IF NOT EXISTS deadline (" <<
			"	id				INTEGER NOT NULL," <<
			"	height			INTEGER NOT NULL," <<
			"	account			INTEGER NOT NULL," <<
			"	nonce			INTEGER NOT NULL," <<
			"	value			INTEGER NOT NULL," <<
			"	file			TEXT NOT NULL," <<
			"	miner			TEXT NOT NULL," <<
			"	totalplotsize	REAL NOT NULL," <<
			"	status			INTEGER NOT NULL," <<
			"	PRIMARY KEY (id)" <<
			")", now;

		*dbSession_ <<
			"CREATE TABLE IF NOT EXISTS block (" <<
			"	id				INTEGER NOT NULL," <<
			"	height			INTEGER NOT NULL," <<
			"	scoop			INTEGER NOT NULL," <<
			"	baseTarget		INTEGER NOT NULL," <<
			"	gensig			TEXT NOT NULL," <<
			"	difficulty		INTEGER NOT NULL," <<
			"	targetDeadline	INTEGER NOT NULL," <<
			"	roundTime		REAL NOT NULL," <<
			"	blockTime		REAL NOT NULL," <<
			"	PRIMARY KEY (id)" <<
			")", now;
	}
	catch (Poco::Exception& e)
	{
		throw Poco::Exception{Poco::format("Could not load/create the database '%s'\n\tReason: %s", databasePath, e.displayText())};
	}

	try
	{
		Poco::UInt64 blocksMined;
		Poco::UInt64 minDifficultyHeight, minDifficulty;
		Poco::UInt64 maxDifficultyHeight, maxDifficulty;
		Poco::UInt64 deadlinesConfirmed;
		Poco::Tuple<Poco::UInt64, Poco::UInt64, Poco::UInt64, Poco::UInt64, std::string, Poco::UInt64, Poco::UInt64> bestDeadline;

		*dbSession_ << "SELECT COUNT(*) FROM block", into(blocksMined), now;
		*dbSession_ << "SELECT height, MIN(difficulty) FROM block", into(minDifficultyHeight), into(minDifficulty), now;
		*dbSession_ << "SELECT height, MAX(difficulty) FROM block", into(maxDifficultyHeight), into(maxDifficulty), now;
		*dbSession_ << "SELECT COUNT(*) FROM deadline WHERE status = 3", into(deadlinesConfirmed), now;
		const auto stmtBestDeadline = (*dbSession_ << "SELECT nonce, value, account, height, file, totalplotsize, MIN(value) FROM deadline", 
			into(bestDeadline), now);

		blocksMined_ = blocksMined;
		lowestDifficulty_ = {minDifficultyHeight, minDifficulty};
		highestDifficulty_ = {maxDifficultyHeight, maxDifficulty};
		deadlinesConfirmed_ = deadlinesConfirmed;

		if (!bestDeadline.get<4>().empty())
		{
			bestDeadlineOverall_ = std::make_shared<Deadline>(
				bestDeadline.get<0>(),
				bestDeadline.get<1>(),
				std::make_shared<Account>(bestDeadline.get<2>()),
				bestDeadline.get<3>(),
				bestDeadline.get<4>(),
				nullptr);

			bestDeadlineOverall_->setTotalPlotsize(bestDeadline.get<5>());
		}

		// load historical blocks
		Poco::UInt64 lastHeight;
		*dbSession_ << "SELECT MAX(height) FROM block", into(lastHeight), now;

		if (lastHeight > 0)
		{
			const auto firstHeight = lastHeight - MinerConfig::getConfig().getMaxHistoricalBlocks();

			std::vector<Poco::UInt64> heights, baseTargets, targetDeadlines;
			std::vector<std::string> gensigs;

			*dbSession_ << "SELECT height, baseTarget, gensig, targetDeadline FROM block " <<
				"			WHERE height >= :from AND height <= :to",
				into(heights), into(baseTargets), into(gensigs), into(targetDeadlines),
				bind(firstHeight), bind(lastHeight), now;

			std::vector<Poco::UInt64> nonces, values, accounts, totalPlotSizes;
			std::vector<std::string> files;
			Poco::UInt64 height;

			auto stmtDeadlines = (*dbSession_ << "SELECT nonce, value, account, file, totalplotsize " <<
												 "FROM deadline " <<
												 "WHERE height = :height",
				into(nonces), into(values), into(accounts), into(files), into(totalPlotSizes), use(height));

			for (size_t i = 0; i < heights.size() && i < MinerConfig::getConfig().getMaxHistoricalBlocks(); ++i)
			{
				height = heights[i];

				auto historicBlock = std::make_shared<BlockData>(
					height,
					baseTargets[i],
					gensigs[i],
					this,
					targetDeadlines[i]
				);

				stmtDeadlines.execute();

				for (size_t j = 0; j < nonces.size(); ++j)
					historicBlock->addDeadline(nonces[j], values[j], std::make_shared<Account>(accounts[j]), height, files[j]);

				historicalBlocks_.emplace_back(std::move(historicBlock));
			}
		}
	}
	catch (Poco::Exception& e)
	{
		throw Poco::Exception{Poco::format("Could not load the data from '%s'\n\tReason: %s", databasePath, e.displayText())};
	}
}

Burst::MinerData::~MinerData() = default;

std::shared_ptr<Burst::BlockData> Burst::MinerData::startNewBlock(Poco::UInt64 block, Poco::UInt64 baseTarget,
                                                                  const std::string& genSig,
                                                                  Poco::UInt64 blockTargetDeadline)
{
	std::unique_lock<std::mutex> lock{ mutex_ };

	// save the old data in the historical container
	if (blockData_ != nullptr)
	{
		const auto maxSize = MinerConfig::getConfig().getMaxHistoricalBlocks();

		// if we reached the maximum size of blocks, forget the oldest
		while (historicalBlocks_.size() + 1 > maxSize)
			historicalBlocks_.pop_front();

		// we clear all entries, because it is also in the logfile
		// and we dont need it in the ram anymore
		blockData_->clearEntries();

		historicalBlocks_.emplace_back(blockData_);

		// add the block to the database
		try
		{
			*dbSession_ <<
				"INSERT INTO block VALUES (NULL, :height, :scoop, :btarget, :gensig, :diff, :targdl, :roundt, :blockt)",
				bind(blockData_->getBlockheight()), bind(blockData_->getScoop()), bind(blockData_->getBasetarget()),
				useRef(blockData_->getGensigStr()), bind(blockData_->getDifficulty()), bind(blockData_->getBlockTargetDeadline()),
				bind(blockData_->getRoundTime()), bind(blockData_->getBlockTime()), now;

			blockData_->forDeadlines([this](const Deadline& deadline)
			{
				try
				{
					const auto status = [&]()
					{
						if (deadline.isConfirmed())
							return 3;

						if (deadline.isSent())
							return 2;

						if (deadline.isOnTheWay())
							return 1;

						return 0;
					}();

					*dbSession_ <<
						"INSERT INTO deadline VALUES (NULL, :height, :account, :nonce, :value, :file, :miner, :totalplotsize, :status)",
						bind(deadline.getBlock()), bind(deadline.getAccountId()), bind(deadline.getNonce()), bind(deadline.getDeadline()),
						bind(deadline.getPlotFile()), bind(deadline.getMiner()), bind(deadline.getTotalPlotsize()), bind(status), now;
				}
				catch (Poco::Exception& e)
				{
					log_error(MinerLogger::general, "Could not insert deadline %s for block %Lu\n\tReason: %s",
						deadline.deadlineToReadableString(), deadline.getBlock(), e.displayText());
				}

				return false;
			});
		}
		catch (Poco::Exception& e)
		{
			log_error(MinerLogger::general, "Could not insert block %Lu\n\tReason: %s",
				blockData_->getBlockheight(), e.displayText());
		}

		++blocksMined_;
	}

	lock.unlock();
	blockData_ = std::make_shared<BlockData>(block, baseTarget, genSig, this, blockTargetDeadline);

	lock.lock();
	currentBlockheight_ = block;
	currentBasetarget_ = baseTarget;
	currentScoopNum_ = blockData_->getScoop();

	const auto lowestDiff = lowestDifficulty_;
	const auto highestDiff = highestDifficulty_;

	if (highestDiff.height == 0 || highestDiff.value < blockData_->getDifficulty())
		highestDifficulty_ = {blockData_->getBlockheight(), blockData_->getDifficulty()};

	if (lowestDiff.height == 0 || lowestDiff.value > blockData_->getDifficulty())
		lowestDifficulty_ = {blockData_->getBlockheight(), blockData_->getDifficulty()};

	return blockData_;
}

Poco::UInt64 Burst::MinerData::runGetWonBlocks(const std::pair<const Wallet*, const Accounts*>& args)
{
	poco_ndc(BlockData::runGetWonBlocks);

	auto& wallet = *args.first;
	auto& accounts = *args.second;
	size_t wonBlocks = 0;

	if (!wallet.isActive())
		return 0;

	std::vector<Block> blocks;

	for (auto& account : accounts.getAccounts())
		if (wallet.getAccountBlocks(account->getId(), blocks))
			wonBlocks += blocks.size();

	bool refresh;

	{
		std::lock_guard<std::mutex> lock(mutex_);
		const auto before = blocksWon_.load();
		blocksWon_.store(wonBlocks);
		refresh = before != wonBlocks;
	}

	if (blockData_ != nullptr && refresh)
		blockData_->refreshBlockEntry();

	return wonBlocks;
}

void Burst::MinerData::addConfirmedDeadline()
{
	++deadlinesConfirmed_;
}

void Burst::MinerData::setBestDeadline(std::shared_ptr<Deadline> deadline)
{
	std::lock_guard<std::mutex> lock{ mutex_ };

	// we check if its the best deadline overall
	if (bestDeadlineOverall_ == nullptr ||
		deadline->getDeadline() < bestDeadlineOverall_->getDeadline())
		bestDeadlineOverall_ = deadline;
}

void Burst::MinerData::addMessage(const Poco::Message& message)
{
	const auto blockData = getBlockData();

	if (blockData != nullptr)
		blockData->addMessage(message);
}

std::shared_ptr<Burst::Deadline> Burst::MinerData::getBestDeadlineOverall(bool onlyHistorical) const
{
	if (!onlyHistorical)
		return bestDeadlineOverall_;

	std::lock_guard<std::mutex> lock(mutex_);

	std::shared_ptr<Deadline> bestHistoricalDeadline;

	for (const auto& block : historicalBlocks_)
		if (block != nullptr && block->getBestDeadline() != nullptr &&
			(bestHistoricalDeadline == nullptr ||
				block->getBestDeadline()->getDeadline() < bestHistoricalDeadline->getDeadline()))
			bestHistoricalDeadline = block->getBestDeadline();

	return bestHistoricalDeadline;
}

const Poco::Timestamp& Burst::MinerData::getStartTime() const
{
	return startTime_;
}

Poco::Timespan Burst::MinerData::getRunTime() const
{
	return Poco::Timestamp{} - getStartTime();
}

void Burst::BlockData::setBlockTime(Poco::UInt64 bTime)
{
	blockTime_ = bTime;
}

Poco::UInt64 Burst::BlockData::getBlockTime() const
{
	return blockTime_;
}

Poco::UInt64 Burst::MinerData::getBlocksMined() const
{
	return blocksMined_;
}

Poco::UInt64 Burst::MinerData::getBlocksWon() const
{
	return blocksWon_.load();
}

std::shared_ptr<Burst::BlockData> Burst::MinerData::getBlockData()
{
	return blockData_;
}

std::shared_ptr<const Burst::BlockData> Burst::MinerData::getBlockData() const
{
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

std::shared_ptr<const Burst::BlockData> Burst::MinerData::getHistoricalBlockData(Poco::UInt32 roundsBefore) const
{
	if (roundsBefore == 0)
		return getBlockData();

	auto wantedBlock = getBlockData()->getBlockheight() - roundsBefore;

	std::lock_guard<std::mutex> lock{ mutex_ };

	const auto iter = std::find_if(historicalBlocks_.begin(), historicalBlocks_.end(), [wantedBlock](const std::shared_ptr<BlockData>& data)
	{
		return data->getBlockheight() == wantedBlock;
	});

	if (iter == historicalBlocks_.end())
		return nullptr;

	return (*iter);
}

std::vector<std::shared_ptr<const Burst::BlockData>> Burst::MinerData::getAllHistoricalBlockData() const
{
	std::lock_guard<std::mutex> lock{ mutex_ };
	
	std::vector<std::shared_ptr<const BlockData>> blocks;
	blocks.reserve(historicalBlocks_.size());

	for (auto& block : historicalBlocks_)
		blocks.emplace_back(block);

	return blocks;
}

Poco::UInt64 Burst::MinerData::getConfirmedDeadlines() const
{
	return deadlinesConfirmed_;
}

Poco::UInt64 Burst::MinerData::getAverageDeadline() const
{
	std::lock_guard<std::mutex> lock{ mutex_ };
	
	Poco::UInt64 avg = 0;
	size_t size = 0;

	if (historicalBlocks_.empty())
		return 0;

	for (const auto& block : historicalBlocks_)
	{
		const auto bestDeadline = block->getBestDeadline();

		if (bestDeadline != nullptr)
		{
			avg += bestDeadline->getDeadline();
			++size;
		}
	}

	if (size == 0)
		return 0;

	return avg / size;
}

Poco::Int64 Burst::MinerData::getDifficultyDifference() const
{
	if (blockData_ == nullptr)
		return 0;

	const Poco::Int64 difficulty = blockData_->getDifficulty();
	const auto lastBlockData = getHistoricalBlockData(1);

	if (lastBlockData == nullptr)
		return difficulty;
	
	return difficulty - lastBlockData->getDifficulty();
}

Burst::HighscoreValue<Poco::UInt64> Burst::MinerData::getLowestDifficulty() const
{
	std::lock_guard<std::mutex> lock{ mutex_ };
	return lowestDifficulty_;
}

Burst::HighscoreValue<Poco::UInt64> Burst::MinerData::getHighestDifficulty() const
{
	std::lock_guard<std::mutex> lock {mutex_};
	return highestDifficulty_;
}

Poco::UInt64 Burst::MinerData::getCurrentBlockheight() const
{
	return currentBlockheight_.load();
}

Poco::UInt64 Burst::MinerData::getCurrentBasetarget() const
{
	return currentBasetarget_.load();
}

Poco::UInt64 Burst::MinerData::getCurrentScoopNum() const
{
	return currentScoopNum_.load();
}

Poco::ActiveResult<Poco::UInt64> Burst::MinerData::getWonBlocksAsync(const Wallet& wallet, const Accounts& accounts)
{
	const auto tuple = std::make_pair(&wallet, &accounts);
	return activityWonBlocks_(tuple);
}
