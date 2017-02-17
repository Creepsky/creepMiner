#include "Deadline.hpp"
#include "MinerUtil.hpp"
#include <algorithm>
#include "nxt/nxt_address.h"
#include "Account.hpp"
#include "MinerData.hpp"

Burst::Deadline::Deadline(uint64_t nonce, uint64_t deadline, std::shared_ptr<Account> account, uint64_t block, std::string plotFile,
                          Deadlines* parent)
	: account_(account),
	  block_(block),
	  nonce_(nonce),
	  deadline_(deadline),
	  plotFile_(std::move(plotFile)),
	  parent_{parent}
{}

uint64_t Burst::Deadline::getNonce() const
{
	return nonce_.load();
}

uint64_t Burst::Deadline::getDeadline() const
{
	return deadline_.load();
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

uint64_t Burst::Deadline::getBlock() const
{
	return block_.load();
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

void Burst::Deadline::setDeadline(uint64_t deadline)
{
	deadline_ = deadline;
}

bool Burst::Deadline::operator<(const Burst::Deadline& rhs) const
{
	Poco::ScopedLock<Poco::FastMutex> lock{ mutex_ };
	return getDeadline() < rhs.getDeadline();
}

Burst::Deadline::~Deadline()
{}

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

Burst::Deadlines::Deadlines(const Deadlines& rhs)
{
	deadlines_ = rhs.deadlines_;
	parent_ = rhs.parent_;
}

std::shared_ptr<Burst::Deadline> Burst::Deadlines::add(uint64_t nonce, uint64_t deadline, std::shared_ptr<Account> account, uint64_t block, std::string plotFile)
{
	Poco::ScopedLock<Poco::FastMutex> lock{ mutex_ };
	
	auto deadlinePtr = std::make_shared<Deadline>(nonce, deadline, account, block, std::move(plotFile), this);

	deadlines_.emplace_back(deadlinePtr);

	std::sort(deadlines_.begin(), deadlines_.end(), [](std::shared_ptr<Deadline> lhs, std::shared_ptr<Deadline> rhs)
			  {
				  return *lhs < *rhs;
			  });

	return deadlinePtr;
}

void Burst::Deadlines::clear()
{
	Poco::ScopedLock<Poco::FastMutex> lock{ mutex_ };
	deadlines_.clear();
}

bool Burst::Deadlines::confirm(Nonce nonce)
{
	Poco::ScopedLock<Poco::FastMutex> lock{ mutex_ };

	auto iter = std::find_if(deadlines_.begin(), deadlines_.end(), [nonce](std::shared_ptr<Deadline> dl)
							 {
								 return dl->getNonce() == nonce;
							 });

	if (iter == deadlines_.end())
		return false;

	(*iter)->confirm();

	return true;
}

bool Burst::Deadlines::confirm(Nonce nonce, AccountId accountId, uint64_t block)
{
	Poco::ScopedLock<Poco::FastMutex> lock{ mutex_ };

	auto iter = std::find_if(deadlines_.begin(), deadlines_.end(), [nonce, accountId, block](std::shared_ptr<Deadline> dl)
	                         {
		                         return dl->getNonce() == nonce && dl->getAccountId() == accountId && dl->getBlock() == block;
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

	return deadlines_.front();
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
	Poco::ScopedLock<Poco::FastMutex> lock{ mutex_ };

	for (auto& deadline : deadlines_)
		if (deadline->isOnTheWay())
			return deadline;

	return nullptr;
}

std::shared_ptr<Burst::Deadline> Burst::Deadlines::getBestSent() const
{
	Poco::ScopedLock<Poco::FastMutex> lock{ mutex_ };

	for (auto& deadline : deadlines_)
		if (deadline->isOnTheWay())
			return deadline;

	return nullptr;
}

void Burst::Deadlines::deadlineEvent(std::shared_ptr<Deadline> deadline, const std::string& type) const
{
	if (parent_ != nullptr)
		parent_->addBlockEntry(createJsonDeadline(*deadline, type));
}

void Burst::Deadlines::deadlineConfirmed(std::shared_ptr<Deadline> deadline) const
{
	if (parent_ != nullptr)
		parent_->confirmedDeadlineEvent(deadline);
}
