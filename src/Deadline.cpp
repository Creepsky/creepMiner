#include "Deadline.hpp"
#include "MinerUtil.hpp"
#include <algorithm>
#include "nxt/nxt_address.h"
#include "Account.hpp"

/*Burst::Deadline::Deadline(uint64_t nonce, uint64_t deadline)
	: nonce(nonce), deadline(deadline)
{}*/

Burst::Deadline::Deadline(uint64_t nonce, uint64_t deadline, std::shared_ptr<Account> account, uint64_t block, std::string plotFile)
	: account_(account), block_(block), nonce_(nonce), deadline_(deadline), plotFile_(std::move(plotFile))
{}

uint64_t Burst::Deadline::getNonce() const
{
	return nonce_;
}

uint64_t Burst::Deadline::getDeadline() const
{
	return deadline_;
}

Burst::AccountId Burst::Deadline::getAccountId() const
{
	return account_->getId();
}

std::string Burst::Deadline::getAccountName() const
{
	auto& name = account_->getName();

	if (name.empty())
		return NxtAddress(getAccountId()).to_string();

	return name;
}

uint64_t Burst::Deadline::getBlock() const
{
	return block_;
}

bool Burst::Deadline::isOnTheWay() const
{
	return sent_;
}

bool Burst::Deadline::isConfirmed() const
{
	return confirmed_;
}

const std::string& Burst::Deadline::getPlotFile() const
{
	return plotFile_;
}

bool Burst::Deadline::operator<(const Burst::Deadline& rhs) const
{
	return getDeadline() < rhs.getDeadline();
}

Burst::Deadline::~Deadline()
{}

void Burst::Deadline::send()
{
	sent_ = true;
}

void Burst::Deadline::confirm()
{
	confirmed_ = true;
}

std::string Burst::Deadline::deadlineToReadableString() const
{
	return deadlineFormat(deadline_);
}

bool Burst::Deadline::operator()(const Deadline& lhs, const Deadline& rhs) const
{
	return lhs.deadline_ < rhs.deadline_;
}

std::shared_ptr<Burst::Deadline> Burst::Deadlines::add(Deadline&& deadline)
{
	auto deadlinePtr = std::make_shared<Deadline>(std::move(deadline));

	deadlines.emplace_back(deadlinePtr);
	std::sort(deadlines.begin(), deadlines.end(), [](std::shared_ptr<Deadline> lhs, std::shared_ptr<Deadline> rhs)
			  {
				  return *lhs < *rhs;
			  });

	return deadlinePtr;
}

void Burst::Deadlines::clear()
{
	deadlines.clear();
}

bool Burst::Deadlines::confirm(Nonce nonce)
{
	auto iter = std::find_if(deadlines.begin(), deadlines.end(), [nonce](std::shared_ptr<Deadline> dl)
							 {
								 return dl->getNonce() == nonce;
							 });

	if (iter == deadlines.end())
		return false;

	(*iter)->confirm();
	return true;
}

bool Burst::Deadlines::confirm(Nonce nonce, AccountId accountId, uint64_t block)
{
	auto iter = std::find_if(deadlines.begin(), deadlines.end(), [nonce, accountId, block](std::shared_ptr<Deadline> dl)
							 {
								 return dl->getNonce() == nonce && dl->getAccountId() == accountId && dl->getBlock() == block;
							 });

	if (iter == deadlines.end())
		return false;

	(*iter)->confirm();
	return true;
}

std::shared_ptr<Burst::Deadline> Burst::Deadlines::getBest() const
{
	if (deadlines.empty())
		return nullptr;
	return deadlines.front();
}

std::shared_ptr<Burst::Deadline> Burst::Deadlines::getBestConfirmed() const
{
	for (auto& deadline : deadlines)
		if (deadline->isConfirmed())
			return deadline;
	return nullptr;
}

std::shared_ptr<Burst::Deadline> Burst::Deadlines::getBestSent() const
{
	for (auto& deadline : deadlines)
		if (deadline->isOnTheWay())
			return deadline;
	return nullptr;
}
