#include "Deadline.hpp"
#include "MinerUtil.h"

Burst::Deadline::Deadline(uint64_t nonce, uint64_t deadline)
	: nonce(nonce), deadline(deadline)
{}

uint64_t Burst::Deadline::getNonce() const
{
	return nonce;
}

uint64_t Burst::Deadline::getDeadline() const
{
	return deadline;
}

bool Burst::Deadline::isConfirmed() const
{
	return confirmed;
}

bool Burst::Deadline::operator<(const Burst::Deadline& rhs) const
{
	return getDeadline() < rhs.getDeadline();
}

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
	deadlines.emplace(std::move(deadline));
}

void Burst::Deadlines::clear()
{
	deadlines.clear();
}

Burst::Deadline* Burst::Deadlines::getBestDeadline()
{
	return nullptr;
}

Burst::Deadline* Burst::Deadlines::getBestConfirmed()
{
	return nullptr;
}
