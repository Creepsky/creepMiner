#include "NonceSubmitter.hpp"
#include "MinerLogger.hpp"
#include "Deadline.hpp"
#include "MinerUtil.hpp"
#include "Request.hpp"
#include "MinerConfig.hpp"
#include "Miner.hpp"
#include <fstream>

Burst::NonceSubmitter::NonceSubmitter(Miner& miner, std::shared_ptr<Deadline> deadline)
	: Task(serializeDeadline(*deadline)), miner(miner), deadline(deadline)
{}

void Burst::NonceSubmitter::runTask()
{
	auto accountName = deadline->getAccountName();
	auto betterDeadlineInPipeline = false;

	auto loopConditionHelper = [this, &betterDeadlineInPipeline](size_t tryCount, size_t maxTryCount, SubmitResponse response)
	{
		if (tryCount >= maxTryCount ||
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

	MinerLogger::write(accountName + ": nonce on the way (" + deadline->deadlineToReadableString() + ")");

	NonceConfirmation confirmation { 0, SubmitResponse::None };
	size_t submitTryCount = 0;
	auto firstSendAttempt = true;

	// submit-loop
	while (loopConditionHelper(submitTryCount,
		MinerConfig::getConfig().getSubmissionMaxRetry(),
		confirmation.errorCode))
	{
		MinerLogger::write("Submit-loop " + std::to_string(submitTryCount + 1) + " (" + deadline->deadlineToReadableString() + ")",
			TextType::Debug);

		NonceRequest request{MinerConfig::getConfig().createSession(HostType::Pool)};

		auto response = request.submit(deadline->getNonce(), deadline->getAccountId());
		auto receiveTryCount = 0u;

		if (response.canReceive() && firstSendAttempt)
		{
			MinerLogger::write(accountName + ": nonce submitted (" + deadlineFormat(deadline->getDeadline()) + ")", TextType::Ok);
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
					MinerLogger::write("Error on logging confirmed deadline!", TextType::Error);
					MinerLogger::write("Path: '" + confirmedDeadlinesPath + "'", TextType::Error);
				}
			}

			auto message = accountName + ": nonce confirmed (" + deadlineFormat(deadline->getDeadline()) + ")";

			if (MinerConfig::getConfig().output.nonceConfirmedPlot)
				message += " in " + deadline->getPlotFile();

			auto bestConfirmed = miner.getBestConfirmed(deadline->getAccountId(), deadline->getBlock());
			auto showConfirmation = true;

			// only show the confirmation, if the confirmed deadline is better then the best already confirmed
			if (bestConfirmed != nullptr)
				showConfirmation = bestConfirmed->getDeadline() > deadline->getDeadline();

			if (showConfirmation)
				MinerLogger::write(message, TextType::Success);

			// we have to confirm it at the very last position
			// because we work with the best confirmed deadline so far before
			// this point
			deadline->confirm();
		}
		else if (betterDeadlineInPipeline)
			MinerLogger::write("Better deadline in pipeline, stop submitting! (" + deadlineFormat(deadline->getDeadline()) + ")",
				TextType::Debug);
		else
		{
			// sent, but not confirmed
			if (firstSendAttempt)
				MinerLogger::write(accountName + ": could not submit nonce! network issues?" +
					" (" + deadlineFormat(deadline->getDeadline()) + ")", TextType::Error);
			else if (confirmation.errorCode == SubmitResponse::Error)
				MinerLogger::write(accountName + ": error on submitting nonce!" +
					" (" + deadlineFormat(deadline->getDeadline()) + ")", TextType::Error);
			else
				MinerLogger::write(accountName + ": got no confirmation from server! busy?" +
					" (" + deadlineFormat(deadline->getDeadline()) + ")", TextType::Error);
		}
	}
	else
	{
		MinerLogger::write("Found nonce was for the last block, stopped submitting! (" +
			deadlineFormat(deadline->getDeadline()) + ")", TextType::Debug);
	}
}
