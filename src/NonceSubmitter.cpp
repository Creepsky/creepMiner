#include "NonceSubmitter.hpp"
#include "MinerLogger.hpp"
#include "Deadline.hpp"
#include "MinerUtil.hpp"
#include "nxt/nxt_address.h"
#include "Request.hpp"
#include "MinerConfig.hpp"
#include "Miner.hpp"
#include <fstream>
#include <atomic>

Burst::NonceSubmitter::NonceSubmitter(Miner& miner, std::shared_ptr<Deadline> deadline)
	: miner_(&miner), deadline_(deadline)
{}

void Burst::NonceSubmitter::startSubmit()
{
	std::thread t(&NonceSubmitter::submitThread, this, miner_, deadline_);
	t.detach();
}

void Burst::NonceSubmitter::submitThread(Miner* miner, std::shared_ptr<Deadline> deadline) const
{
	static std::atomic_uint submitThreads = 0;

	++submitThreads;

	// if max submit threads is set, check current amount of submit threads
	if (MinerConfig::getConfig().getMaxSubmitThreads() > 0 &&
		submitThreads > MinerConfig::getConfig().getMaxSubmitThreads())
	{
		// we cant start another submit thread, because all slots are designated
		MinerLogger::write(NxtAddress(deadline->getAccountId()).to_string() + ": too many submit threads running! (" +
			std::to_string(submitThreads) + ")");
		--submitThreads;
		// stop submitting
		return;
	}

	MinerLogger::write(std::to_string(submitThreads) + " submitter-threads running", TextType::Debug);

	auto nonce = deadline->getNonce();
	auto deadlineValue = deadline->getDeadline();
	auto accountId = deadline->getAccountId();
	auto accountName = deadline->getAccountName();
	auto betterDeadlineInPipeline = false;
	auto plotFile = deadline->getPlotFile();

	auto loopConditionHelper = [deadline, miner, &betterDeadlineInPipeline](size_t tryCount, size_t maxTryCount, SubmitResponse response)
			{
				if (tryCount >= maxTryCount ||
					response != SubmitResponse::None ||
					deadline->getBlock() != miner->getBlockheight() ||
					betterDeadlineInPipeline)
					return false;

				auto bestSent = miner->getBestSent(deadline->getAccountId(), deadline->getBlock());
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

		auto response = request.submit(nonce, accountId);
		auto receiveTryCount = 0u;

		if (response.canReceive() && firstSendAttempt)
		{
			MinerLogger::write(accountName + ": nonce submitted (" + deadlineFormat(deadlineValue) + ")", TextType::Ok);
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
	if (deadline->getBlock() == miner->getBlockheight())
	{
		if (confirmation.errorCode == SubmitResponse::Submitted)
		{
			deadline->confirm();

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

			auto message = accountName + ": nonce confirmed (" + deadlineFormat(deadlineValue) + ")";

			if (MinerConfig::getConfig().output.nonceConfirmedPlot)
				message += " in " + plotFile;

			MinerLogger::write(message, TextType::Success);
		}
		else if (betterDeadlineInPipeline)
			MinerLogger::write("Better deadline in pipeline, stop submitting! (" + deadlineFormat(deadline->getDeadline()) + ")",
							   TextType::Debug);
		else
		{
			// sent, but not confirmed
			if (firstSendAttempt)
				MinerLogger::write(accountName + ": could not submit nonce! network issues?" +
								   " (" + deadlineFormat(deadlineValue) + ")", TextType::Error);
			else if (confirmation.errorCode == SubmitResponse::Error)
				MinerLogger::write(accountName + ": error on submitting nonce!" +
					" (" + deadlineFormat(deadlineValue) + ")", TextType::Error);
			else
				MinerLogger::write(accountName + ": got no confirmation from server! busy?" +
								   " (" + deadlineFormat(deadlineValue) + ")", TextType::Error);
		}
	}
	else
	{
		MinerLogger::write("Found nonce was for the last block, stopped submitting! (" +
						   deadlineFormat(deadlineValue) + ")", TextType::Debug);
	}

	--submitThreads;

	MinerLogger::write(std::to_string(submitThreads) + " submitter-threads running", TextType::Debug);
}

