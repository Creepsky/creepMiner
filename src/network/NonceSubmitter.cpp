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

#include "NonceSubmitter.hpp"
#include "logging/MinerLogger.hpp"
#include "mining/Deadline.hpp"
#include "MinerUtil.hpp"
#include "Request.hpp"
#include "mining/MinerConfig.hpp"
#include "mining/Miner.hpp"
#include "logging/Output.hpp"
#include <chrono>
#include <thread>
#include <Poco/JSON/Parser.h>

Burst::NonceSubmitter::NonceSubmitter(Miner& miner, const std::shared_ptr<Deadline>& deadline)
	: Task(serializeDeadline(*deadline)),
	  miner_(miner),
	  deadline_(deadline)
{}

void Burst::NonceSubmitter::runTask()
{
	// ReSharper disable CppExpressionWithoutSideEffects
	submit();
	// ReSharper restore CppExpressionWithoutSideEffects
}

Burst::NonceConfirmation Burst::NonceSubmitter::submit() const
{
	return submit(deadline_, miner_);
}

Burst::NonceConfirmation Burst::NonceSubmitter::submit(const std::shared_ptr<Deadline>& deadline, const Miner& miner)
{
	auto accountName = deadline->getAccountName();
	NonceConfirmation confirmation{0, SubmitResponse::None, "", 0, ""};
	Poco::JSON::Parser jsonParser;

	const auto& altUrls = MinerConfig::getConfig().getPoolUrlAlt();
	std::vector<Url> urls;
	urls.reserve(altUrls.size() + 1);
	urls.emplace_back(MinerConfig::getConfig().getPoolUrl());
	urls.insert(urls.end(), altUrls.begin(), altUrls.end());
	auto urlIter = urls.begin();

	const auto loopConditionHelper = [&deadline, &miner, &confirmation, &urls](unsigned& tryCount,
	                                                              unsigned maxTryCount,
	                                                              std::vector<Url>::iterator* urlIter)
	{
		if (confirmation.errorCode == SubmitResponse::Confirmed ||
			confirmation.errorCode == SubmitResponse::NotBest ||
			deadline->getBlock() != miner.getBlockheight())
			return false;

		if ((maxTryCount > 0 && tryCount >= maxTryCount) || confirmation.errorCode == SubmitResponse::Error)
		{
			if (urlIter != nullptr)
			{
				++*urlIter;

				if (*urlIter != urls.end())
				{
					tryCount = 0;
					return true;
				}
			}

			return false;
		}

		const auto bestSent = miner.getBestSent(deadline->getAccountId(), deadline->getBlock());

		if (bestSent != nullptr)
		{
			if (bestSent->getDeadline() < deadline->getDeadline())
				confirmation.errorCode = SubmitResponse::NotBest;
				
			//MinerLogger::write("Best sent nonce so far: " + bestSent->deadlineToReadableString() + " vs. this deadline: "
			//+ deadlineFormat(deadline->getDeadline()), TextType::Debug);
		}

		if (confirmation.errorCode == SubmitResponse::NotBest)
		{
			log_debug(MinerLogger::nonceSubmitter, deadline->toActionString("nonce discarded - not best"));
			return false;
		}
			
		return true;
	};

	//MinerLogger::write("sending nonce from thread, " + deadlineFormat(deadlineValue), TextType::System);

	unsigned submitTryCount = 0;
	auto firstSendAttempt = true;
	const auto submissionMaxRetry = MinerConfig::getConfig().getSubmissionMaxRetry();

	// submit-loop
	while (loopConditionHelper(submitTryCount, submissionMaxRetry, &urlIter))
	{
		NonceRequest request{MinerConfig::getConfig().createSession(*urlIter)};

		auto response = request.submit(*deadline);
		auto receiveTryCount = 0u;

		if (response.canReceive())
		{
			if (firstSendAttempt)
			{
				deadline->send();
				confirmation.errorCode = SubmitResponse::Submitted;
				log_ok_if(MinerLogger::nonceSubmitter, MinerLogger::hasOutput(NonceSent), deadline->toActionString("nonce submitted"));
				firstSendAttempt = false;
			}

			while (loopConditionHelper(receiveTryCount,
			                           MinerConfig::getConfig().getReceiveMaxRetry(),
			                           nullptr))
			{
				confirmation = response.getConfirmation();
				++receiveTryCount;
			}
		}
		else
		{
			confirmation.errorCode = SubmitResponse::Error;

			log_debug(MinerLogger::nonceSubmitter, deadline->toActionString(Poco::format("no connection to %s %u/%u",
				urlIter->getCanonical(), submitTryCount, submissionMaxRetry)));
		}

		++submitTryCount;
		
		if (confirmation.errorCode != SubmitResponse::Confirmed &&
			confirmation.errorCode != SubmitResponse::NotBest &&
			confirmation.errorCode != SubmitResponse::TooHigh &&
			confirmation.errorCode != SubmitResponse::WrongBlock)
			std::this_thread::sleep_for(std::chrono::seconds(3));
	}
	
	// it has to be the same block
	if (deadline->getBlock() == miner.getBlockheight())
	{
		if (confirmation.errorCode == SubmitResponse::Confirmed)
		{
			// our calculated deadlines differs from the pools one
			if (confirmation.deadline != deadline->getDeadline())
			{
				log_error(MinerLogger::nonceSubmitter,
					"The pool calculated a different deadline for your nonce than your miner has!\n"
					"\tPlot file: %s\n"
					"\tPools deadline: %s\n"
					"\tYour deadline: %s",
					deadline->getPlotFile(),
					deadlineFormat(confirmation.deadline),
					deadlineFormat(deadline->getDeadline()));

				// change the deadline
				deadline->setDeadline(confirmation.deadline);
			}
			else
			{
				auto bestConfirmed = miner.getBestConfirmed(deadline->getAccountId(), deadline->getBlock());
				//auto showConfirmation = true;

				// it is better to show all confirmations...
					// only show the confirmation, if the confirmed deadline is better then the best already confirmed
					//if (bestConfirmed != nullptr)
					//	showConfirmation = bestConfirmed->getDeadline() > deadline->getDeadline();

				//if (showConfirmation)
				log_success_if(MinerLogger::nonceSubmitter, MinerLogger::hasOutput(NonceConfirmed), deadline->toActionString("nonce confirmed"));

				// we have to confirm it at the very last position
				// because we work with the best confirmed deadline so far before
				// this point
				deadline->confirm();
			}
		}
		else if (confirmation.errorCode == SubmitResponse::NotBest)
			log_debug(MinerLogger::nonceSubmitter, deadline->toActionString("nonce discarded - not best"));
		else
		{
			std::vector<std::pair<std::string, std::string>> errorDescription;

			if (confirmation.errorNumber > 0)
				errorDescription.emplace_back("error-code", std::to_string(confirmation.errorNumber));

			if (confirmation.errorText.empty())
			{
				if (!confirmation.json.empty())
					errorDescription.emplace_back("error-text", confirmation.json);
			}
			else
				errorDescription.emplace_back("error-text", confirmation.errorText);

			// sent, but not confirmed
			if (firstSendAttempt)
				log_warning(MinerLogger::nonceSubmitter, deadline->toActionString("could not submit nonce! This is probably a network issue.", errorDescription));
			else if (confirmation.errorCode == SubmitResponse::Error)
				log_error(MinerLogger::nonceSubmitter, deadline->toActionString("error on submitting nonce!", errorDescription));
			else
				log_warning(MinerLogger::nonceSubmitter, deadline->toActionString("got no confirmation from pool!", errorDescription));
		}
	}
	else
	{
		log_debug(MinerLogger::nonceSubmitter, deadline->toActionString("found nonce was for the last block, stopped submitting!"));
	}

	return confirmation;
}
