#include "Deadline.hpp"
#include "MinerUtil.h"
#include <algorithm>

Burst::Deadline::Deadline(uint64_t nonce, uint64_t deadline)
	: nonce(nonce), deadline(deadline)
{}

Burst::Deadline::Deadline(uint64_t nonce, uint64_t deadline, AccountId accountId, uint64_t block)
	: accountId(accountId), block(block), nonce(nonce), deadline(deadline)
{}

uint64_t Burst::Deadline::getNonce() const
{
	return nonce;
}

uint64_t Burst::Deadline::getDeadline() const
{
	return deadline;
}

Burst::AccountId Burst::Deadline::getAccountId() const
{
	return accountId;
}

uint64_t Burst::Deadline::getBlock() const
{
	return block;
}

bool Burst::Deadline::isConfirmed() const
{
	return confirmed;
}

bool Burst::Deadline::operator<(const Burst::Deadline& rhs) const
{
	return getDeadline() < rhs.getDeadline();
}

Burst::Deadline::~Deadline()
{}

void Burst::Deadline::confirm()
{
	confirmed = true;
}

std::string Burst::Deadline::deadlineToReadableString() const
{
	return deadlineFormat(deadline);
}

bool Burst::Deadline::operator()(const Deadline& lhs, const Deadline& rhs) const
{
	return lhs.deadline < rhs.deadline;
}

void Burst::Deadlines::add(Deadline&& deadline)
{
	deadlines.emplace_back(std::make_shared<Deadline>(std::move(deadline)));
	std::sort(deadlines.begin(), deadlines.end(), [](std::shared_ptr<Deadline> lhs, std::shared_ptr<Deadline> rhs)
			  {
				  return *lhs < *rhs;
			  });
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

std::shared_ptr<Burst::Deadline> Burst::Deadlines::getBestDeadline() const
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
