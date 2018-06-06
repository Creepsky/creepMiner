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

#include "Deadline.hpp"
#include "MinerUtil.hpp"
#include <algorithm>
#include <utility>
#include "nxt/nxt_address.h"
#include "wallet/Account.hpp"
#include "MinerData.hpp"
#include "plots/PlotSizes.hpp"
#include "MinerConfig.hpp"

Burst::Deadline::Deadline(Deadline&& other) noexcept
	: account_{std::move(other.account_)}, block_{other.block_}, nonce_{other.nonce_},
	  deadline_{other.deadline_}, plotFile_{std::move(other.plotFile_)}, onTheWay_{other.onTheWay_.load()},
	  sent_{other.sent_.load()},
	  confirmed_{other.confirmed_.load()}, minerName_{std::move(other.minerName_)},
	  workerName_{std::move(other.workerName_)}, plotsize_{other.plotsize_}, ip_{std::move(other.ip_)},
	  parent_{other.parent_}
{
}

Burst::Deadline& Burst::Deadline::operator=(Deadline&& other) noexcept
{
	if (this == &other)
		return *this;
	account_ = std::move(other.account_);
	block_ = other.block_;
	nonce_ = other.nonce_;
	deadline_ = other.deadline_;
	plotFile_ = std::move(other.plotFile_);
	onTheWay_ = other.onTheWay_.load();
	sent_ = other.sent_.load();
	confirmed_ = other.confirmed_.load();
	minerName_ = std::move(other.minerName_);
	workerName_ = std::move(other.workerName_);
	plotsize_ = other.plotsize_;
	ip_ = std::move(other.ip_);
	parent_ = other.parent_;
	return *this;
}

Burst::Deadline::Deadline(const Poco::UInt64 nonce, const Poco::UInt64 deadline, std::shared_ptr<Account> account,
                          const Poco::UInt64 block, std::string plotFile, Deadlines* parent)
	: account_(std::move(account)),
	  block_(block),
	  nonce_(nonce),
	  deadline_(deadline),
	  plotFile_(std::move(plotFile)),
	  onTheWay_{false},
	  sent_{false},
	  confirmed_{false},
	  parent_{parent}
{}

Poco::UInt64 Burst::Deadline::getNonce() const
{
	return nonce_;
}

Poco::UInt64 Burst::Deadline::getDeadline() const
{
	return deadline_;
}

Burst::AccountId Burst::Deadline::getAccountId() const
{
	return account_->getId();
}

std::string Burst::Deadline::getAccountName() const
{
	auto name = account_->getName();

	if (name.empty())
		return NxtAddress(getAccountId()).to_string();

	return name;
}

Poco::UInt64 Burst::Deadline::getBlock() const
{
	return block_;
}

bool Burst::Deadline::isOnTheWay() const
{
	return onTheWay_.load();
}

bool Burst::Deadline::isSent() const
{
	return sent_.load();
}

bool Burst::Deadline::isConfirmed() const
{
	return confirmed_.load();
}

const std::string& Burst::Deadline::getPlotFile() const
{
	Poco::ScopedLock<Poco::FastMutex> lock{ mutex_ };
	return plotFile_;
}

std::string Burst::Deadline::getMiner() const
{
	return minerName_.empty() ? Settings::project.nameAndVersionVerbose : minerName_;
}

const std::string& Burst::Deadline::getWorker() const
{
	return workerName_;
}

Poco::UInt64 Burst::Deadline::getTotalPlotsize() const
{
	return plotsize_ == 0 ? PlotSizes::getTotal(PlotSizes::Type::Combined) : plotsize_;
}

const Poco::Net::IPAddress& Burst::Deadline::getIp() const
{
	return ip_;
}

std::string Burst::Deadline::toActionString(const std::string& action) const
{
	if (MinerConfig::getConfig().isVerboseLogging())
		return Poco::format("%s: %s (%s)\n"
		                    "\tnonce: %s\n"
		                    "\tin:    %s\n"
		                    "\tfrom:  %s",
		                    getAccountName(), action, deadlineToReadableString(), numberToString(getNonce()), getPlotFile(),
		                    getWorker().empty() ? getIp().toString() : Poco::format("%s (%s)", getWorker(), getIp().toString()));

	return Poco::format("%s: %s (%s)", getAccountName(), action, deadlineToReadableString(), numberToString(getNonce()));
}

std::string Burst::Deadline::toActionString(const std::string& action,
	const std::vector<std::pair<std::string, std::string>>& additionalData) const
{
	size_t longestKey = 0;

	for (const auto& pair : additionalData)
		if (pair.first.size() > longestKey)
			longestKey = pair.first.size();

	longestKey = std::max(longestKey, size_t{5});
	
	std::stringstream sstream;
	sstream << Poco::format("%s: %s (%s)", getAccountName(), action, deadlineToReadableString()) << std::endl;
	sstream << '\t' << Poco::format("nonce:%s%s", std::string(longestKey + 3 - 5, ' '), numberToString(getNonce())) << std::endl;
	sstream << '\t' << Poco::format("in:%s%s", std::string(longestKey + 3 - 2, ' '), getPlotFile()) << std::endl;
	sstream << '\t' << Poco::format("from:%s%s",
	                                std::string(longestKey + 3 - 4, ' '),
	                                getWorker().empty()
		                                ? getIp().toString()
		                                : Poco::format("%s (%s)", getWorker(), getIp().toString())) << std::endl;

	for (size_t i = 0; i < additionalData.size(); ++i)
	{
		const auto& pair = additionalData[i];

		sstream << '\t' << Poco::format("%s:%s%s", pair.first, std::string(longestKey + 3 - pair.first.size(), ' '),
		                                pair.second);

		if (i + 1 < additionalData.size())
			sstream << std::endl;		
	}

	return sstream.str();
}

void Burst::Deadline::setDeadline(Poco::UInt64 deadline)
{
	deadline_ = deadline;

	if (parent_ != nullptr)
		parent_->resort();
}

void Burst::Deadline::setMiner(const std::string& miner)
{
	minerName_ = miner;
}

void Burst::Deadline::setWorker(const std::string& worker)
{
	workerName_ = worker;
}

void Burst::Deadline::setTotalPlotsize(const Poco::UInt64 plotsize)
{
	plotsize_ = plotsize;
}

void Burst::Deadline::setIp(const Poco::Net::IPAddress& ip)
{
	ip_ = ip;
}

void Burst::Deadline::setParent(Deadlines* parent)
{
	parent_ = parent;
}

bool Burst::Deadline::operator<(const Burst::Deadline& rhs) const
{
	return getDeadline() < rhs.getDeadline();
}

Burst::Deadline::~Deadline() = default;

void Burst::Deadline::found(const bool tooHigh)
{
	if (parent_ != nullptr)
		parent_->deadlineEvent(this->shared_from_this(), std::string("nonce found") + (tooHigh ? " (too high)" : + ""));
}

void Burst::Deadline::onTheWay()
{
	onTheWay_ = true;

	if (parent_ != nullptr)
		parent_->deadlineEvent(this->shared_from_this(), "nonce found");
}

void Burst::Deadline::send()
{	
	sent_ = true;

	if (parent_ != nullptr)
		parent_->deadlineEvent(this->shared_from_this(), "nonce submitted");
}

void Burst::Deadline::confirm()
{	
	confirmed_ = true;

	if (parent_ != nullptr)
		parent_->deadlineConfirmed(this->shared_from_this());
}

std::string Burst::Deadline::deadlineToReadableString() const
{
	return deadlineFormat(deadline_);
}

bool Burst::Deadline::operator()(const Deadline& lhs, const Deadline& rhs) const
{
	return lhs.deadline_ < rhs.deadline_;
}

Burst::Deadlines::Deadlines(BlockData* parent)
	: parent_{parent}
{}

std::shared_ptr<Burst::Deadline> Burst::Deadlines::add(Poco::UInt64 nonce, Poco::UInt64 deadline, std::shared_ptr<Account> account, Poco::UInt64 block, std::string plotFile)
{
	Poco::ScopedLock<Poco::FastMutex> lock{mutex_};
	auto deadlinePtr = std::make_shared<Deadline>(nonce, deadline, account, block, std::move(plotFile), this);
	deadlines_.insert(deadlinePtr);
	return deadlinePtr;
}

void Burst::Deadlines::add(std::shared_ptr<Deadline> deadline)
{
	Poco::ScopedLock<Poco::FastMutex> lock{mutex_};
	deadline->setParent(this);
	deadlines_.emplace(std::move(deadline));
}

void Burst::Deadlines::clear()
{
	Poco::ScopedLock<Poco::FastMutex> lock{ mutex_ };
	deadlines_.clear();
}

bool Burst::Deadlines::confirm(Nonce nonce)
{
	Poco::ScopedLock<Poco::FastMutex> lock{ mutex_ };

	const auto iter = std::find_if(deadlines_.begin(), deadlines_.end(), [nonce](std::shared_ptr<Deadline> dl)
	{
		return dl->getNonce() == nonce;
	});

	if (iter == deadlines_.end())
		return false;

	(*iter)->confirm();

	return true;
}

bool Burst::Deadlines::confirm(Nonce nonce, AccountId accountId, Poco::UInt64 block)
{
	Poco::ScopedLock<Poco::FastMutex> lock{ mutex_ };

	const auto iter = std::find_if(deadlines_.begin(), deadlines_.end(),
	                               [nonce, accountId, block](std::shared_ptr<Deadline> dl)
	                               {
		                               return dl->getNonce() == nonce && dl->getAccountId() == accountId && dl->getBlock() ==
			                               block;
	                               });

	if (iter == deadlines_.end())
		return false;

	(*iter)->confirm();
	return true;
}

std::shared_ptr<Burst::Deadline> Burst::Deadlines::getBest() const
{
	Poco::ScopedLock<Poco::FastMutex> lock{ mutex_ };

	if (deadlines_.empty())
		return nullptr;

	return *deadlines_.begin();
}

std::shared_ptr<Burst::Deadline> Burst::Deadlines::getBestConfirmed() const
{
	Poco::ScopedLock<Poco::FastMutex> lock{ mutex_ };

	for (auto& deadline : deadlines_)
		if (deadline->isConfirmed())
			return deadline;

	return nullptr;
}

std::shared_ptr<Burst::Deadline> Burst::Deadlines::getBestFound() const
{
	return getBest();
}

std::shared_ptr<Burst::Deadline> Burst::Deadlines::getBestSent() const
{
	Poco::ScopedLock<Poco::FastMutex> lock{ mutex_ };

	for (auto& deadline : deadlines_)
		if (deadline->isSent())
			return deadline;

	return nullptr;
}

std::vector<std::shared_ptr<Burst::Deadline>> Burst::Deadlines::getDeadlines() const
{
	Poco::ScopedLock<Poco::FastMutex> lock{mutex_};
	std::vector<std::shared_ptr<Deadline>> deadlines;

	for (auto& deadline : deadlines_)
		deadlines.emplace_back(deadline);

	return deadlines;
}

void Burst::Deadlines::deadlineEvent(const std::shared_ptr<Deadline>& deadline, const std::string& type) const
{
	if (parent_ != nullptr)
		parent_->addBlockEntry(createJsonDeadline(*deadline, type));
}

void Burst::Deadlines::deadlineConfirmed(const std::shared_ptr<Deadline>& deadline) const
{
	if (parent_ != nullptr)
		parent_->confirmedDeadlineEvent(deadline);
}

void Burst::Deadlines::resort()
{
	Poco::ScopedLock<Poco::FastMutex> lock{ mutex_ };
	deadlines_ = { deadlines_.begin(), deadlines_.end() };
}
