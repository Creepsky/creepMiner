//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#include "Miner.hpp"
#include "MinerLogger.hpp"
#include "MinerConfig.hpp"
#include "PlotReader.hpp"
#include "MinerUtil.hpp"
#include "nxt/nxt_address.h"
#include "Response.hpp"
#include "Request.hpp"
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/JSON/Object.h>
#include "NonceSubmitter.hpp"
#include <Poco/JSON/Parser.h>
#include <Poco/NestedDiagnosticContext.h>
#include "PlotVerifier.hpp"
#include "PlotSizes.hpp"
#include "Output.hpp"

Burst::Miner::Miner()
{}

Burst::Miner::~Miner()
{}

void Burst::Miner::run()
{
	poco_ndc(Miner::run);
	running_ = true;
	progress_ = std::make_shared<PlotReadProgress>();

	auto& config = MinerConfig::getConfig();
	auto errors = 0u;

	if (config.getPoolUrl().empty() &&
		config.getMiningInfoUrl().empty() &&
		config.getWalletUrl().empty())
	{
		log_critical(MinerLogger::miner, "Pool/Wallet/MiningInfo are all empty! Miner has nothing to do and shutting down!");
		return;
	}

	log_system(MinerLogger::miner, "Total plots size: %s", memToString(MinerConfig::getConfig().getTotalPlotsize(), 2));
	log_system(MinerLogger::miner, "Submission Max Retry : %s", (config.getSubmissionMaxRetry() == 0u ? "unlimited" :
		                   std::to_string(config.getSubmissionMaxRetry())));
	log_system(MinerLogger::miner, "Buffer Size : %z MB", config.maxBufferSizeMB);
	if (!config.getPoolUrl().empty())
		log_system(MinerLogger::miner, "Pool Host : %s:%hu (%s)",
			config.getPoolUrl().getCanonical(), config.getPoolUrl().getPort(), config.getPoolUrl().getIp());
	if (!config.getMiningInfoUrl().empty())
		log_system(MinerLogger::miner, "Mininginfo URL : %s:%hu (%s)",
			config.getMiningInfoUrl().getCanonical(), config.getMiningInfoUrl().getPort(), config.getMiningInfoUrl().getIp());
	if (!config.getWalletUrl().empty())
		log_system(MinerLogger::miner, "Wallet URL : %s:%hu (%s)",
			config.getWalletUrl().getCanonical(), config.getWalletUrl().getPort(), config.getWalletUrl().getIp());
	if (config.getStartServer() && !config.getServerUrl().empty())
		log_system(MinerLogger::miner, "Server URL : %s:%hu (%s)",
			config.getServerUrl().getCanonical(), config.getServerUrl().getPort(), config.getServerUrl().getIp());
	if (config.getTargetDeadline() > 0)
		log_system(MinerLogger::miner, "Target deadline : %s", deadlineFormat(config.getTargetDeadline()));
	log_system(MinerLogger::miner, "Mining intensity : %u", config.getMiningIntensity());
	log_system(MinerLogger::miner, "Max plot readers : %z",
		(config.getMaxPlotReaders() == 0
		? config.getPlotList().size()
		: config.getMaxPlotReaders()));
	log_system(MinerLogger::miner, "Log path : %s", MinerConfig::getConfig().getPathLogfile().toString());

	// only create the thread pools and manager for mining if there is work to do (plot files)
	if (!config.getPlotFiles().empty())
	{
		// manager
		nonceSubmitterManager_ = std::make_unique<Poco::TaskManager>();

		// create the plot verifiers
		verifier_ = std::make_unique<WorkerList<PlotVerifier>>(Poco::Thread::Priority::PRIO_HIGHEST, MinerConfig::getConfig().getMiningIntensity(),
			*this, verificationQueue_);

		auto verifiers = verifier_->size();

		if (verifiers != MinerConfig::getConfig().getMiningIntensity())
		{
			log_critical(MinerLogger::miner, "Could not create all verifiers (%z/%z)!\n"
				"Lower the setting \"maxBufferSizeMB\"!", verifiers, MinerConfig::getConfig().getMiningIntensity());

			if (verifiers == 0)
				return;
		}

		// create plot reader
		size_t plotReaderToCreate = MinerConfig::getConfig().getMaxPlotReaders();

		if (plotReaderToCreate == 0)
			plotReaderToCreate = MinerConfig::getConfig().getPlotList().size();

		plotReader_ = std::make_unique<WorkerList<PlotReader>>(Poco::Thread::Priority::PRIO_HIGHEST, plotReaderToCreate,
			*this, progress_, verificationQueue_, plotReadQueue_);

		auto plotReader = plotReader_->size();

		//auto plotReader = createWorkers<PlotReader>(plotReaderToCreate, *plotReaderThreadPool_,
			//Poco::Thread::Priority::PRIO_HIGHEST, *this, progress_, verificationQueue_, plotReadQueue_);

		if (plotReader != plotReaderToCreate)
		{
			log_critical(MinerLogger::miner, "Could not create all plot reader (%z/%z)!\n"
				"Lower the setting \"maxPlotReaders\"!", plotReader, plotReaderToCreate);

			if (plotReader == 0)
				return;
		}
	}

	wallet_ = MinerConfig::getConfig().getWalletUrl();

	const auto sleepTime = std::chrono::seconds(3);
	running_ = true;

	miningInfoSession_ = MinerConfig::getConfig().createSession(HostType::MiningInfo);
	miningInfoSession_->setKeepAlive(true);

	wallet_.getLastBlock(currentBlockHeight_);

	while (running_)
	{
		if (getMiningInfo())
			errors = 0;
		else
		{
			++errors;
			log_debug(MinerLogger::miner, "Could not get mining infos %u/5 times...", errors);
		}

		// we have a tollerance of 5 times of not being able to fetch mining infos, before its a real error
		if (errors >= 5)
		{
			// reset error-counter and show error-message in console
			log_error(MinerLogger::miner, "Could not get block infos!");
			errors = 0;
		}

		std::this_thread::sleep_for(sleepTime);
	}

	running_ = false;
}

void Burst::Miner::stop()
{
	poco_ndc(Miner::stop);
	plotReadQueue_.wakeUpAll();
	plotReader_->stop();
	verificationQueue_.wakeUpAll();
	verifier_->stop();
	Poco::ThreadPool::defaultPool().stopAll();
	running_ = false;
}

void Burst::Miner::updateGensig(const std::string gensigStr, uint64_t blockHeight, uint64_t baseTarget)
{
	poco_ndc(Miner::updateGensig);

	// stop all reading processes if any
	if (!MinerConfig::getConfig().getPlotFiles().empty())
	{
		plotReadQueue_.clear();
		verificationQueue_.clear();
		PlotReader::sumBufferSize_ = 0;
		log_debug(MinerLogger::miner, "Verification queue cleared.");
	}

	log_debug(MinerLogger::miner, "Locking threads...");
	Poco::ScopedLock<Poco::FastMutex> lock { deadlinesLock_ };
	log_debug(MinerLogger::miner, "Threads locked, setting up new block %Lu...", blockHeight);

	// new round, clear all deadlines
	deadlines_.clear();

	for (auto i = 0; i < 32; ++i)
	{
		auto byteStr = gensigStr.substr(i * 2, 2);
		this->gensig_[i] = static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16));
	}

	gensigStr_ = gensigStr;

	GensigData newGenSig;
	hash.update(&gensig_[0], gensig_.size());
	hash.update(blockHeight);
	hash.close(&newGenSig[0]);
	auto scoopNum = (static_cast<int>(newGenSig[newGenSig.size() - 2] & 0x0F) << 8) | static_cast<int>(newGenSig[newGenSig.size() - 1]);

	// setup new block-data
	data_.startNewBlock(blockHeight, scoopNum, baseTarget, gensigStr);

	// why we start a new thread to gather the last winner:
	// it could be slow and is not necessary for the whole process
	// so show it when it's done
	if (currentBlockHeight_ > 0 &&
		!MinerConfig::getConfig().getWalletUrl().getIp().empty())
	{
		// copy blockheight temporary for lambda arg capture
		auto block = data_.getCurrentBlock() - 1;

		std::thread threadLastWinner([this, block]()
			{
				poco_ndc(Miner::updateGensig::threadLastWinner);
				AccountId lastWinner;
				auto fetched = false;

				// we cheat here a little bit and use the submission max retry set in config
				// for max retry fetching the last winner
				for (auto loop = 0u;
					 !fetched && (loop < MinerConfig::getConfig().getSubmissionMaxRetry() || MinerConfig::getConfig().getSubmissionMaxRetry() == 0);
					 ++loop)
				{
					if (wallet_.getWinnerOfBlock(block, lastWinner))
					{
						std::string name;

						// we are the winner :)
						if (accounts_.isLoaded(lastWinner))
						{
							data_.addWonBlock();
							// we (re)send the good news to the local html server
							data_.addBlockEntry(createJsonNewBlock(data_));
						}

						wallet_.getNameOfAccount(lastWinner, name);

						// if we dont fetched the winner of the last block
						// we exit this thread function
						if (data_.getCurrentBlock() - 1 != block)
							return;

						auto address = NxtAddress(lastWinner).to_string();

						auto lastWinnerPtr = std::make_shared<Poco::JSON::Object>();
						lastWinnerPtr->set("type", "lastWinner");
						lastWinnerPtr->set("numeric", lastWinner);
						lastWinnerPtr->set("address", address);

						if (!name.empty())
						{
							lastWinnerPtr->set("name", name);
						}

						log_ok_if(MinerLogger::miner, MinerLogger::hasOutput(LastWinner), std::string(50, '-') + "\n"
							"last block winner: \n"
							"block#             %Lu\n"
							"winner-numeric     %Lu\n"
							"winner-address     %s\n"
							"%s" +
							std::string(50, '-'),
							block, lastWinner, address, (name.empty() ? "" : Poco::format("winner-name        %s\n", name))
						);

						data_.getBlockData()->lastWinner = lastWinnerPtr;
						data_.addBlockEntry(*lastWinnerPtr);

						fetched = true;
					}
				}
			});

		threadLastWinner.detach();
	}

	// printing block info and transfer it to local server
	{
		log_notice(MinerLogger::miner, std::string(50, '-') + "\n"
			"block#      %Lu\n"
			"scoop#      %d\n"
			"baseTarget# %Lu\n" +
			std::string(50, '-'),
			blockHeight, scoopNum, baseTarget
		);

		data_.addBlockEntry(createJsonNewBlock(data_));
	}

	progress_->reset();
	progress_->setMax(MinerConfig::getConfig().getTotalPlotsize());

	PlotSizes::nextRound();
	PlotSizes::refresh(MinerConfig::getConfig().getPlotsHash());

	for (auto& plotDir : MinerConfig::getConfig().getPlotList())
	{
		auto plotRead = new PlotReadNotification;
		plotRead->dir = plotDir.first;
		plotRead->plotList = plotDir.second;
		plotRead->gensig = getGensig();
		plotRead->scoopNum = getScoopNum();
		plotRead->blockheight = getBlockheight();

		plotReadQueue_.enqueueNotification(plotRead);
	}
}

const Burst::GensigData& Burst::Miner::getGensig() const
{
	return this->gensig_;
}

const std::string& Burst::Miner::getGensigStr() const
{
	return gensigStr_;
}

uint64_t Burst::Miner::getBaseTarget() const
{
	auto blockData = data_.getBlockData();

	if (blockData == nullptr)
		return 0;

	return data_.getBlockData()->baseTarget;
}

uint64_t Burst::Miner::getBlockheight() const
{
	return data_.getCurrentBlock();
}

uint64_t Burst::Miner::getTargetDeadline() const
{
	auto targetDeadline = targetDeadline_;
	auto manualTargetDeadline = MinerConfig::getConfig().getTargetDeadline();

	if (targetDeadline == 0)
		targetDeadline = manualTargetDeadline;
	else if (targetDeadline > manualTargetDeadline &&
		manualTargetDeadline > 0)
		targetDeadline = manualTargetDeadline;

	return targetDeadline;
}

size_t Burst::Miner::getScoopNum() const
{
	return data_.getBlockData()->scoop;
}

void Burst::Miner::submitNonce(uint64_t nonce, uint64_t accountId, uint64_t deadline, std::string plotFile)
{
	poco_ndc(Miner::submitNonce);
	Poco::ScopedLock<Poco::FastMutex> mutex { deadlinesLock_ };

	auto bestDeadline = deadlines_[accountId].getBest();

	//MinerLogger::write("nonce found with deadline: " + deadlineFormat(deadline), TextType::Debug);

	// is the new nonce better then the best one we already have?
	if (bestDeadline == nullptr || bestDeadline->getDeadline() > deadline)
	{
		auto newDeadline = deadlines_[accountId].add({ nonce,
			deadline,
			accounts_.getAccount(accountId, wallet_, true),
			data_.getCurrentBlock(),
			plotFile
		});

		data_.addBlockEntry(createJsonDeadline(newDeadline, "nonce found"));

		log_unimportant_if(MinerLogger::miner, MinerLogger::hasOutput(NonceFound), "%s: nonce found (%s)\n\tin: %s",
			newDeadline->getAccountName(), deadlineFormat(deadline), plotFile);

		auto createSendThread = true;
		auto targetDeadline = getTargetDeadline();

		if (targetDeadline > 0 && newDeadline->getDeadline() >= targetDeadline)
		{
			createSendThread = false;

			log_debug(MinerLogger::miner, "Nonce is higher then the target deadline of the pool (%s)",
				deadlineFormat(targetDeadline));
		}

		newDeadline->send();

		if (createSendThread)
#ifdef NDEBUG
			nonceSubmitterManager_->start(new NonceSubmitter{ *this, newDeadline });
#else
		{} // in debug mode we dont submit nonces
#endif
	}
}

bool Burst::Miner::getMiningInfo()
{
	using namespace Poco::Net;
	poco_ndc(Miner::getMiningInfo);

	Request request(std::move(miningInfoSession_));

	HTTPRequest requestData { HTTPRequest::HTTP_GET, "/burst?requestType=getMiningInfo", HTTPRequest::HTTP_1_1 };
	requestData.setKeepAlive(true);

	auto response = request.send(requestData);
	std::string responseData;

	if (response.receive(responseData))
	{
		try
		{
			HttpResponse httpResponse(responseData);
			Poco::JSON::Parser parser;
			auto root = parser.parse(httpResponse.getMessage()).extract<Poco::JSON::Object::Ptr>();
			std::string gensig;

			if (root->has("baseTarget"))
			{
				std::string baseTargetStr = root->get("baseTarget");
				currentBaseTarget_ = std::stoull(baseTargetStr);
			}

			if (root->has("generationSignature"))
			{
				gensig = root->get("generationSignature").convert<std::string>();
			}

			if (root->has("targetDeadline"))
			{
				targetDeadline_ = root->get("targetDeadline");
			}

			if (root->has("height"))
			{
				std::string newBlockHeightStr = root->get("height");
				auto newBlockHeight = std::stoull(newBlockHeightStr);

				if (newBlockHeight > currentBlockHeight_)
					updateGensig(gensig, newBlockHeight, currentBaseTarget_);

				currentBlockHeight_ = newBlockHeight;
			}

			transferSession(response, miningInfoSession_);
			return true;
		}
		catch (Poco::Exception& exc)
		{
			std::vector<std::string> lines = {
				"Error on getting new block-info!",
				exc.what(),
				"Full response: " + responseData
			};

			log_error(MinerLogger::miner, "Error on getting new block-info!\n\t%s\n\tFull response:\n\t%s", exc.displayText(), responseData);
			log_current_stackframe(MinerLogger::miner);
		}
	}

	transferSession(request, miningInfoSession_);
	transferSession(response, miningInfoSession_);

	return false;
}

std::shared_ptr<Burst::Deadline> Burst::Miner::getBestSent(uint64_t accountId, uint64_t blockHeight)
{
	poco_ndc(Miner::getBestSent);
	Poco::ScopedLock<Poco::FastMutex> mutex { deadlinesLock_ };

	if (blockHeight != data_.getCurrentBlock())
		return nullptr;

	return deadlines_[accountId].getBestSent();
}

std::shared_ptr<Burst::Deadline> Burst::Miner::getBestConfirmed(uint64_t accountId, uint64_t blockHeight)
{
	poco_ndc(Miner::getBestConfirmed);
	Poco::ScopedLock<Poco::FastMutex> mutex { deadlinesLock_ };

	if (blockHeight != data_.getCurrentBlock())
		return nullptr;

	return deadlines_[accountId].getBestConfirmed();
}

std::vector<Poco::JSON::Object> Burst::Miner::getBlockData() const
{
	poco_ndc(Miner::getBlockData);
	std::vector<Poco::JSON::Object> entries;
	auto blockData = data_.getBlockData();

	if (blockData == nullptr)
		return { };

	entries.reserve(blockData->entries.size());

	for (auto& entry : blockData->entries)
		entries.emplace_back(entry);

	return entries;
}

Burst::MinerData& Burst::Miner::getData()
{
	return data_;
}
