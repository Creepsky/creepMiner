#pragma once

#include "Declarations.hpp"

struct CalculatedDeadline
{
	uint64_t deadline;
	uint64_t nonce;
};

extern "C" void calculate_shabal_cuda(Burst::ScoopData* buffer, uint64_t len,
	const Burst::GensigData* gensig, CalculatedDeadline* calculatedDeadlines,
	uint64_t nonceStart, uint64_t nonceRead, uint64_t baseTarget);
