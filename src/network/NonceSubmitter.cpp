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
#include <fstream>
#include "logging/Output.hpp"
#include <chrono>
#include <thread>

Burst::NonceSubmitter::NonceSubmitter(Miner& miner, const std::shared_ptr<Deadline>& deadline)
	: Task(serializeDeadline(*deadline)),
	  submitAsync(this, &NonceSubmitter::submit),
	  miner_(miner),
	  deadline_(deadline)
{}

void Burst::NonceSubmitter::runTask()
{
	submit();
}

Burst::NonceConfirmation Burst::NonceSubmitter::submit()
{
	auto accountName = deadline_->getAccountName();
	auto betterDeadlineInPipeline = false;

	const auto loopConditionHelper = [this, &betterDeadlineInPipeline](unsigned tryCount, unsigned maxTryCount, SubmitResponse response)
	{
		if ((maxTryCount > 0 && tryCount >= maxTryCount) ||
			response == SubmitResponse::Error ||
			response == SubmitResponse::Confirmed ||
			deadline_->getBlock() != miner_.getBlockheight() ||
			betterDeadlineInPipeline)
			return false;

		const auto bestSent = miner_.getBestSent(deadline_->getAccountId(), deadline_->getBlock());
		betterDeadlineInPipeline = false;

		if (bestSent != nullptr)
		{
			betterDeadlineInPipeline = bestSent->getDeadline() < deadline_->getDeadline();
			//MinerLogger::write("Best sent nonce so far: " + bestSent->deadlineToReadableString() + " vs. this deadline: "
			//+ deadlineFormat(deadline->getDeadline()), TextType::Debug);
		}

		if (betterDeadlineInPipeline)
		{
			log_debug(MinerLogger::nonceSubmitter, deadline_->toActionString("nonce discarded - not best"));
			return false;
		}
			
		return true;
	};

	//MinerLogger::write("sending nonce from thread, " + deadlineFormat(deadlineValue), TextType::System);

	NonceConfirmation confirmation{0, SubmitResponse::None, ""};
	unsigned submitTryCount = 0;
	auto firstSendAttempt = true;

	// submit-loop
	while (loopConditionHelper(submitTryCount, MinerConfig::getConfig().getSubmissionMaxRetry(), confirmation.errorCode))
	{
		log_debug(MinerLogger::nonceSubmitter, deadline_->toActionString(Poco::format("submit-loop %u", submitTryCount + 1)));

		if (submitTryCount)
		{
			log_debug(MinerLogger::nonceSubmitter,"Waiting 5 seconds till next submission...");
			std::this_thread::sleep_for(std::chrono::seconds(5));
		}

		NonceRequest request{MinerConfig::getConfig().createSession(HostType::Pool)};

		auto response = request.submit(*deadline_);
		auto receiveTryCount = 0u;

		if (response.canReceive() && firstSendAttempt)
		{
			deadline_->send();
			confirmation.errorCode = SubmitResponse::Submitted;
			log_ok_if(MinerLogger::nonceSubmitter, MinerLogger::hasOutput(NonceSent), deadline_->toActionString("nonce submitted"));
			firstSendAttempt = false;
		}

		while (loopConditionHelper(receiveTryCount,
			MinerConfig::getConfig().getReceiveMaxRetry(),
			confirmation.errorCode))
		{
			confirmation = response.getConfirmation();
			++receiveTryCount;
		}

		++submitTryCount;
	}
	
	// it has to be the same block
	if (deadline_->getBlock() == miner_.getBlockheight())
	{
		if (confirmation.errorCode == SubmitResponse::Confirmed)
		{
			// our calculated deadlines differs from the pools one
			if (confirmation.deadline != deadline_->getDeadline())
			{
				log_error(MinerLogger::nonceSubmitter,
					"The pool calculated a different deadline for your nonce than your miner has!\n"
					"\tPlot file: %s\n"
					"\tPools deadline: %s\n"
					"\tYour deadline: %s",
					deadline_->getPlotFile(),
					deadlineFormat(confirmation.deadline),
					deadlineFormat(deadline_->getDeadline()));

				// change the deadline
				deadline_->setDeadline(confirmation.deadline);
			}
			else
			{
				auto bestConfirmed = miner_.getBestConfirmed(deadline_->getAccountId(), deadline_->getBlock());
				//auto showConfirmation = true;

				// it is better to show all confirmations...
					// only show the confirmation, if the confirmed deadline is better then the best already confirmed
					//if (bestConfirmed != nullptr)
					//	showConfirmation = bestConfirmed->getDeadline() > deadline->getDeadline();

				//if (showConfirmation)
				log_success_if(MinerLogger::nonceSubmitter, MinerLogger::hasOutput(NonceConfirmed), deadline_->toActionString("nonce confirmed"));

				// we have to confirm it at the very last position
				// because we work with the best confirmed deadline so far before
				// this point
				deadline_->confirm();
			}
		}
		else if (betterDeadlineInPipeline)
			log_debug(MinerLogger::nonceSubmitter, "Better deadline in pipeline, stop submitting! (%s)", deadlineFormat(deadline_->getDeadline()));
		else
		{
			// sent, but not confirmed
			if (firstSendAttempt)
				log_warning(MinerLogger::nonceSubmitter, deadline_->toActionString("could not submit nonce! This is probably a network issue."));
			else if (confirmation.errorCode == SubmitResponse::Error)
				log_error(MinerLogger::nonceSubmitter, deadline_->toActionString("error on submitting nonce!"));
			else
				log_warning(MinerLogger::nonceSubmitter, deadline_->toActionString(
					"got no confirmation from pool! It is probably to busy, please set your submissionMaxRetry a bit higher if you see this."
				));
		}
	}
	else
	{
		log_debug(MinerLogger::nonceSubmitter, deadline_->toActionString("found nonce was for the last block, stopped submitting!"));
	}

	return confirmation;
}
