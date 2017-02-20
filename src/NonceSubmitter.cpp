#include "NonceSubmitter.hpp"
#include "MinerLogger.hpp"
#include "Deadline.hpp"
#include "MinerUtil.hpp"
#include "Request.hpp"
#include "MinerConfig.hpp"
#include "Miner.hpp"
#include <fstream>
#include "Output.hpp"

Burst::NonceSubmitter::NonceSubmitter(Miner& miner, std::shared_ptr<Deadline> deadline)
	: Task(serializeDeadline(*deadline)), miner(miner), deadline(deadline)
{}

void Burst::NonceSubmitter::runTask()
{
	auto accountName = deadline->getAccountName();
	auto betterDeadlineInPipeline = false;

	auto loopConditionHelper = [this, &betterDeadlineInPipeline](size_t tryCount, size_t maxTryCount, SubmitResponse response)
	{
		if (maxTryCount > 0 && tryCount >= maxTryCount ||
			response != SubmitResponse::None ||
			deadline->getBlock() != miner.getBlockheight() ||
			betterDeadlineInPipeline ||
			isCancelled())
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

	log_information_if(MinerLogger::nonceSubmitter, MinerLogger::hasOutput(NonceOnTheWay), "%s: nonce on the way (%s)",
		accountName, deadline->deadlineToReadableString());

	NonceConfirmation confirmation { 0, SubmitResponse::None };
	size_t submitTryCount = 0;
	auto firstSendAttempt = true;
	
	// submit-loop
	while (loopConditionHelper(submitTryCount,
		MinerConfig::getConfig().getSubmissionMaxRetry(),
		confirmation.errorCode))
	{
		log_debug(MinerLogger::nonceSubmitter, "Submit-loop %z (%s)", submitTryCount + 1, deadline->deadlineToReadableString());

		NonceRequest request{MinerConfig::getConfig().createSession(HostType::Pool)};

		auto response = request.submit(deadline->getNonce(), deadline->getAccountId());
		auto receiveTryCount = 0u;

		if (response.canReceive() && firstSendAttempt)
		{
			deadline->send();
			log_ok_if(MinerLogger::nonceSubmitter, MinerLogger::hasOutput(NonceSent), "%s: nonce submitted (%s)\n"
				"\tnonce: %Lu\n"
				"\tin %s",
				accountName, deadlineFormat(deadline->getDeadline()), deadline->getNonce(), deadline->getPlotFile());
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
	if (deadline->getBlock() == miner.getBlockheight())
	{
		log_trace(MinerLogger::nonceSubmitter, "miner.getBlockheight");

		if (confirmation.errorCode == SubmitResponse::Submitted)
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
				log_error(MinerLogger::nonceSubmitter, "The pools deadline for the nonce is different then ours!\n"
					"\tPools deadline: %s\n\tOur deadline: %s",
					deadlineFormat(confirmation.deadline), deadlineFormat(deadline->getDeadline()));

				// change the deadline
				deadline->setDeadline(confirmation.deadline);
			}

			log_trace(MinerLogger::nonceSubmitter, "miner.getBestConfirmed >> 0");
			auto bestConfirmed = miner.getBestConfirmed(deadline->getAccountId(), deadline->getBlock());
			log_trace(MinerLogger::nonceSubmitter, "miner.getBestConfirmed >> 1");
			auto showConfirmation = true;

			// only show the confirmation, if the confirmed deadline is better then the best already confirmed
			if (bestConfirmed != nullptr)
				showConfirmation = bestConfirmed->getDeadline() > deadline->getDeadline();

			if (showConfirmation)
				log_success_if(MinerLogger::nonceSubmitter, MinerLogger::hasOutput(NonceConfirmed), "%s: nonce confirmed (%s)\n"
					"\tnonce: %Lu\n"
					"\tin %s",
					accountName, deadlineFormat(deadline->getDeadline()), deadline->getNonce(), deadline->getPlotFile());

			// we have to confirm it at the very last position
			// because we work with the best confirmed deadline so far before
			// this point
			log_trace(MinerLogger::nonceSubmitter, "deadline->confirm >> 0");
			deadline->confirm();
			log_trace(MinerLogger::nonceSubmitter, "deadline->confirm >> 1");
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
}
