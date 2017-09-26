// ==========================================================================
// 
// creepMiner - Burstcoin cryptocurrency CPU and GPU miner
// Copyright (C)  2016-2017 Creepsky (creepsky@gmail.com)
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

Burst::NonceSubmitter::NonceSubmitter(Miner& miner, std::shared_ptr<Deadline> deadline)
	: Task(serializeDeadline(*deadline)),
	  submitAsync(this, &NonceSubmitter::submit),
	  miner(miner),
	  deadline(deadline)
{}

void Burst::NonceSubmitter::runTask()
{
	submit();
}

Burst::NonceConfirmation Burst::NonceSubmitter::submit()
{
	auto accountName = deadline->getAccountName();
	auto betterDeadlineInPipeline = false;

	auto loopConditionHelper = [this, &betterDeadlineInPipeline](unsigned tryCount, unsigned maxTryCount, SubmitResponse response)
	{
		if ((maxTryCount > 0 && tryCount >= maxTryCount) ||
			response == SubmitResponse::Error ||
			response == SubmitResponse::Confirmed ||
			deadline->getBlock() != miner.getBlockheight() ||
			betterDeadlineInPipeline)
			return false;

		auto bestSent = miner.getBestSent(deadline->getAccountId(), deadline->getBlock());
		betterDeadlineInPipeline = false;

		if (bestSent != nullptr)
		{
			betterDeadlineInPipeline = bestSent->getDeadline() < deadline->getDeadline();
			//MinerLogger::write("Best sent nonce so far: " + bestSent->deadlineToReadableString() + " vs. this deadline: "
			//+ deadlineFormat(deadline->getDeadline()), TextType::Debug);
		}

		if (betterDeadlineInPipeline)
			return false;

		return true;
	};

	//MinerLogger::write("sending nonce from thread, " + deadlineFormat(deadlineValue), TextType::System);

	NonceConfirmation confirmation { 0, SubmitResponse::None };
	unsigned submitTryCount = 0;
	auto firstSendAttempt = true;

	// submit-loop
	while (loopConditionHelper(submitTryCount,
		MinerConfig::getConfig().getSubmissionMaxRetry(),
		confirmation.errorCode))
	{
		log_debug(MinerLogger::nonceSubmitter, "Submit-loop %z (%s)", submitTryCount + 1, deadline->deadlineToReadableString());

		if (submitTryCount)
		{
			log_debug(MinerLogger::nonceSubmitter,"WAITING......................");
			std::this_thread::sleep_for(std::chrono::seconds(5));
		}

		NonceRequest request{MinerConfig::getConfig().createSession(HostType::Pool)};

		auto response = request.submit(*deadline);
		auto receiveTryCount = 0u;

		if (response.canReceive() && firstSendAttempt)
		{
			deadline->send();
			confirmation.errorCode = SubmitResponse::Submitted;
			log_ok_if(MinerLogger::nonceSubmitter, MinerLogger::hasOutput(NonceSent), "%s: nonce submitted (%s)\n"
				"\tnonce: %Lu\n"
				"\tin:    %s",
				accountName, deadlineFormat(deadline->getDeadline()),
				deadline->getNonce(),
				deadline->getPlotFile());
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

	log_debug(MinerLogger::nonceSubmitter, "JSON confirmation (%s)\n\t%s", deadline->deadlineToReadableString(), confirmation.json);

	// it has to be the same block
	if (deadline->getBlock() == miner.getBlockheight())
	{
		if (confirmation.errorCode == SubmitResponse::Confirmed)
		{
			auto confirmedDeadlinesPath = MinerConfig::getConfig().getConfirmedDeadlinesPath();

			if (!confirmedDeadlinesPath.empty())
			{
				std::ofstream file;

				file.open(MinerConfig::getConfig().getConfirmedDeadlinesPath(), std::ios_base::out | std::ios_base::app);

				if (file.is_open())
				{
					file << deadline->getAccountId() << ";"
						<< deadline->getPlotFile() << ";"
						<< deadline->getDeadline() << std::endl;

					file.close();
				}
				else
				{
					log_error(MinerLogger::nonceSubmitter, "Error on logging confirmed deadline!\nPath: %s", confirmedDeadlinesPath);
				}
			}

			// our calculated deadlines differs from the pools one
			if (confirmation.deadline != deadline->getDeadline())
			{
				log_error(MinerLogger::nonceSubmitter,
					"The pool calculated a different deadline for your nonce than your miner has!\n"
					"\tPools deadline: %s\n"
					"\tYour deadline: %s",
					deadlineFormat(confirmation.deadline), deadlineFormat(deadline->getDeadline()));

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
					log_success_if(MinerLogger::nonceSubmitter, MinerLogger::hasOutput(NonceConfirmed), "%s: nonce confirmed (%s)\n"
						"\tnonce: %Lu\n"
						"\tin:    %s",
						accountName, deadlineFormat(deadline->getDeadline()), deadline->getNonce(), deadline->getPlotFile());

				// we have to confirm it at the very last position
				// because we work with the best confirmed deadline so far before
				// this point
				deadline->confirm();
			}
		}
		else if (betterDeadlineInPipeline)
			log_debug(MinerLogger::nonceSubmitter, "Better deadline in pipeline, stop submitting! (%s)", deadlineFormat(deadline->getDeadline()));
		else
		{
			// sent, but not confirmed
			if (firstSendAttempt)
				log_warning(MinerLogger::nonceSubmitter, "%s: could not submit nonce! network issues? (%s)",
					accountName, deadlineFormat(deadline->getDeadline()));
			else if (confirmation.errorCode == SubmitResponse::Error)
				log_warning(MinerLogger::nonceSubmitter, "%s: error on submitting nonce! (%s)",
					accountName, deadlineFormat(deadline->getDeadline()));
			else
				log_warning(MinerLogger::nonceSubmitter, "%s: got no confirmation from server! busy? (%s)",
					accountName, deadlineFormat(deadline->getDeadline()));
		}
	}
	else
	{
		log_debug(MinerLogger::nonceSubmitter, "Found nonce was for the last block, stopped submitting! (%s)",
			deadlineFormat(deadline->getDeadline()));
	}

	return confirmation;
}
