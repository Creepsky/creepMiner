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
#include "PlotSizes.hpp"
#include "Output.hpp"

namespace Burst
{
	namespace MinerHelper
	{
		template <typename T, typename ...Args>
		void create_worker(std::unique_ptr<Poco::ThreadPool>& thread_pool, std::unique_ptr<Poco::TaskManager>& task_manager,
			size_t size, Args&&... args)
		{
			thread_pool = std::make_unique<Poco::ThreadPool>(1, static_cast<int>(size));
			task_manager = std::make_unique<Poco::TaskManager>(*thread_pool);

			for (auto i = 0u; i < size; ++i)
				task_manager->start(new T(std::forward<Args>(args)...));
		}
	}
}

Burst::Miner::Miner()
	: submitNonceAsync{this, &Miner::submitNonceAsyncImpl}
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

	MinerConfig::getConfig().printConsole();

	// only create the thread pools and manager for mining if there is work to do (plot files)
	if (!config.getPlotFiles().empty())
	{
		// manager
		nonceSubmitterManager_ = std::make_unique<Poco::TaskManager>();

		// create the plot readers
		MinerHelper::create_worker<PlotReader>(plot_reader_pool_, plot_reader_, MinerConfig::getConfig().getMaxPlotReaders(),
			*this, progress_, verificationQueue_, plotReadQueue_);

		// create the plot verifiers
		MinerHelper::create_worker<PlotVerifier>(verifier_pool_, verifier_, MinerConfig::getConfig().getMiningIntensity(),
			*this, verificationQueue_);
	}

	wallet_ = MinerConfig::getConfig().getWalletUrl();

	const auto sleepTime = std::chrono::seconds(3);
	running_ = true;

	miningInfoSession_ = MinerConfig::getConfig().createSession(HostType::MiningInfo);
	miningInfoSession_->setKeepAlive(true);

	// TODO REWORK
	//wallet_.getLastBlock(currentBlockHeight_);

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
	// stop plot reader
	shut_down_worker(*plot_reader_pool_, *plot_reader_, plotReadQueue_);
	// stop verifier
	shut_down_worker(*verifier_pool_, *verifier_, verificationQueue_);
	// also stop all running background-tasks
	Poco::ThreadPool::defaultPool().stopAll();
	Poco::ThreadPool::defaultPool().joinAll();
	running_ = false;
}

void Burst::Miner::updateGensig(const std::string gensigStr, Poco::UInt64 blockHeight, Poco::UInt64 baseTarget)
{
	poco_ndc(Miner::updateGensig);

	// stop all reading processes if any
	if (!MinerConfig::getConfig().getPlotFiles().empty())
	{
		log_debug(MinerLogger::miner, "Plot-read-queue: %d, verification-queue: %d",
			plotReadQueue_.size(), verificationQueue_.size());
		log_debug(MinerLogger::miner, "Allocated memory: %s", memToString(PlotReader::globalBufferSize.getSize(), 1));
		
		PlotReader::globalBufferSize.setMax(MinerConfig::getConfig().getMaxBufferSize() * 1024 * 1024);
	}
	
	// clear the plot read queue
	plotReadQueue_.clear();

	// setup new block-data
	auto block = data_.startNewBlock(blockHeight, baseTarget, gensigStr);

	// why we start a new thread to gather the last winner:
	// it could be slow and is not necessary for the whole process
	// so show it when it's done
	if (blockHeight > 0 && wallet_.isActive())
		block->getLastWinnerAsync(wallet_, accounts_);

	// printing block info and transfer it to local server
	{
		log_notice(MinerLogger::miner, std::string(50, '-') + "\n"
			"block#      %Lu\n"
			"scoop#      %Lu\n"
			"baseTarget# %Lu\n" +
			std::string(50, '-'),
			blockHeight, block->getScoop(), baseTarget
		);

		data_.getBlockData()->refreshBlockEntry();
	}

	progress_->reset(blockHeight, MinerConfig::getConfig().getTotalPlotsize());

	PlotSizes::nextRound();
	PlotSizes::refresh(MinerConfig::getConfig().getPlotsHash());

	const auto initPlotReadNotification = [this](PlotDir& plotDir)
	{
		auto notification = new PlotReadNotification;
		notification->dir = plotDir.getPath();
		notification->gensig = getGensig();
		notification->scoopNum = getScoopNum();
		notification->blockheight = getBlockheight();
		notification->baseTarget = getBaseTarget();
		notification->type = plotDir.getType();
		return notification;
	};

	const auto addParallel = [this, &initPlotReadNotification](PlotDir& plotDir, std::shared_ptr<PlotFile> plotFile)
	{
		auto plotRead = initPlotReadNotification(plotDir);
		plotRead->plotList.emplace_back(plotFile);
		plotReadQueue_.enqueueNotification(plotRead);
	};

	MinerConfig::getConfig().forPlotDirs([this, &addParallel, &initPlotReadNotification](PlotDir& plotDir)
	{
		if (plotDir.getType() == PlotDir::Type::Parallel)
		{
			for (const auto& plotFile : plotDir.getPlotfiles())
				addParallel(plotDir, plotFile);

			for (const auto& relatedPlotDir : plotDir.getRelatedDirs())
				for (const auto& plotFile : relatedPlotDir->getPlotfiles())
					addParallel(plotDir, plotFile);
		}
		else
		{
			auto plotRead = initPlotReadNotification(plotDir);
			plotRead->plotList = plotDir.getPlotfiles();

			for (const auto& relatedPlotDir : plotDir.getRelatedDirs())
				plotRead->relatedPlotLists.emplace_back(relatedPlotDir->getPath(), relatedPlotDir->getPlotfiles());

			plotReadQueue_.enqueueNotification(plotRead);
		}

		return true;
	});
}

const Burst::GensigData& Burst::Miner::getGensig() const
{
	auto blockData = data_.getBlockData();

	if (blockData == nullptr)
	{
		log_current_stackframe(MinerLogger::miner);
		throw Poco::NullPointerException{"Tried to access the gensig from a nullptr!"};
	}

	return blockData->getGensig();
}

const std::string& Burst::Miner::getGensigStr() const
{
	auto blockData = data_.getBlockData();

	if (blockData == nullptr)
	{
		log_current_stackframe(MinerLogger::miner);
		throw Poco::NullPointerException{"Tried to access the gensig from a nullptr!"};
	}

	return blockData->getGensigStr();
}

Poco::UInt64 Burst::Miner::getBaseTarget() const
{
	return data_.getCurrentBasetarget();
}

Poco::UInt64 Burst::Miner::getBlockheight() const
{
	return data_.getCurrentBlockheight();
}

Poco::UInt64 Burst::Miner::getTargetDeadline() const
{
	return data_.getTargetDeadline();
}

Poco::UInt64 Burst::Miner::getScoopNum() const
{
	return data_.getCurrentScoopNum();
}

Burst::SubmitResponse Burst::Miner::addNewDeadline(Poco::UInt64 nonce, Poco::UInt64 accountId, Poco::UInt64 deadline, Poco::UInt64 blockheight,
	std::string plotFile, std::shared_ptr<Burst::Deadline>& newDeadline)
{
	newDeadline = nullptr;

	if (blockheight != getBlockheight())
		return SubmitResponse::WrongBlock;

	auto targetDeadline = getTargetDeadline();

	if (targetDeadline > 0 && deadline > targetDeadline)
		return SubmitResponse::TooHigh;

	auto block = data_.getBlockData();

	if (block == nullptr)
		return SubmitResponse::Error;

	auto bestDeadline = block->getBestDeadline(accountId, BlockData::DeadlineSearchType::Found);

	newDeadline = block->addDeadlineIfBest(
		nonce,
		deadline,
		accounts_.getAccount(accountId, wallet_, true),
		data_.getBlockData()->getBlockheight(),
		plotFile
	);

	if (newDeadline)
	{
		log_unimportant_if(MinerLogger::miner, MinerLogger::hasOutput(NonceFound), "%s: nonce found (%s)\n"
			"\tnonce: %Lu\n"
			"\tin: %s",
			newDeadline->getAccountName(), deadlineFormat(deadline), newDeadline->getNonce(), plotFile);

		return SubmitResponse::Found;
	}

	return SubmitResponse::Error;
}

void Burst::Miner::submitNonce(Poco::UInt64 nonce, Poco::UInt64 accountId, Poco::UInt64 deadline, Poco::UInt64 blockheight, std::string plotFile)
{
	poco_ndc(Miner::submitNonce);
	
	std::shared_ptr<Deadline> newDeadline;

	auto result = addNewDeadline(nonce, accountId, deadline, blockheight, plotFile, newDeadline);

	// is the new nonce better then the best one we already have?
	if (result == SubmitResponse::Found)
	{
		newDeadline->onTheWay();
		nonceSubmitterManager_->start(new NonceSubmitter{ *this, newDeadline });
	}
}

bool Burst::Miner::getMiningInfo()
{
	using namespace Poco::Net;
	poco_ndc(Miner::getMiningInfo);

	if (miningInfoSession_ == nullptr && !MinerConfig::getConfig().getMiningInfoUrl().empty())
		miningInfoSession_ = MinerConfig::getConfig().getMiningInfoUrl().createSession();

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

			if (root->has("height"))
			{
				std::string newBlockHeightStr = root->get("height");
				auto newBlockHeight = std::stoull(newBlockHeightStr);

				if (data_.getBlockData() == nullptr ||
					newBlockHeight > data_.getBlockData()->getBlockheight())
				{
					std::string baseTargetStr;

					if (root->has("baseTarget"))
						baseTargetStr = root->get("baseTarget").convert<std::string>();

					if (root->has("generationSignature"))
						gensig = root->get("generationSignature").convert<std::string>();

					if (root->has("targetDeadline"))
					{
						// remember the current pool target deadline
						auto target_deadline_pool_before = data_.getTargetDeadline(TargetDeadlineType::Pool);

						// update the new pool target deadline
						data_.setTargetDeadline(root->get("targetDeadline").convert<Poco::UInt64>());

						// if its changed, print it
						if (target_deadline_pool_before != data_.getTargetDeadline(TargetDeadlineType::Pool))
							log_system(MinerLogger::config,
								"got new target deadline\n"
								"\told target deadline: %s\n"
								"\tnew target deadline: %s\n"
								"\tlowest target deadline: %s",
								deadlineFormat(target_deadline_pool_before),
								deadlineFormat(data_.getTargetDeadline(TargetDeadlineType::Pool)),
								deadlineFormat(data_.getTargetDeadline()));
					}

					updateGensig(gensig, newBlockHeight, std::stoull(baseTargetStr));
				}
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

void Burst::Miner::shut_down_worker(Poco::ThreadPool& thread_pool, Poco::TaskManager& task_manager, Poco::NotificationQueue& queue) const
{
	Poco::Mutex::ScopedLock lock(worker_mutex_);
	queue.wakeUpAll();
	task_manager.cancelAll();
	thread_pool.stopAll();
	thread_pool.joinAll();
}

Burst::NonceConfirmation Burst::Miner::submitNonceAsyncImpl(const std::tuple<Poco::UInt64, Poco::UInt64, Poco::UInt64, Poco::UInt64, std::string>& data)
{
	poco_ndc(Miner::submitNonceImpl);

	auto nonce = std::get<0>(data);
	auto accountId = std::get<1>(data);
	auto deadline = std::get<2>(data);
	auto blockheight = std::get<3>(data);
	auto plotFile = std::get<4>(data);

	std::shared_ptr<Deadline> newDeadline;

	auto result = addNewDeadline(nonce, accountId, deadline, blockheight, plotFile, newDeadline);

	// is the new nonce better then the best one we already have?
	if (result == SubmitResponse::Found)
	{
		newDeadline->onTheWay();
		return NonceSubmitter{ *this, newDeadline }.submit();
	}

	NonceConfirmation nonceConfirmation;
	nonceConfirmation.deadline = 0;
	nonceConfirmation.json = Poco::format("{ \"result\" : \"success\", \"deadline\" : %Lu }", deadline);
	nonceConfirmation.errorCode = result;

	return nonceConfirmation;
}

std::shared_ptr<Burst::Deadline> Burst::Miner::getBestSent(Poco::UInt64 accountId, Poco::UInt64 blockHeight)
{
	poco_ndc(Miner::getBestSent);

	auto block = data_.getBlockData();

	if (block == nullptr ||
		blockHeight != block->getBlockheight())
		return nullptr;

	return block->getBestDeadline(accountId, BlockData::DeadlineSearchType::Sent);
}

std::shared_ptr<Burst::Deadline> Burst::Miner::getBestConfirmed(Poco::UInt64 accountId, Poco::UInt64 blockHeight)
{
	poco_ndc(Miner::getBestConfirmed);

	auto block = data_.getBlockData();

	if (block == nullptr ||
		blockHeight != block->getBlockheight())
		return nullptr;

	return block->getBestDeadline(accountId, BlockData::DeadlineSearchType::Confirmed);
}

Burst::MinerData& Burst::Miner::getData()
{
	return data_;
}

std::shared_ptr<Burst::Account> Burst::Miner::getAccount(AccountId id)
{
	if (!accounts_.isLoaded(id))
		return nullptr;

	return accounts_.getAccount(id, wallet_, true);
}

void Burst::Miner::setMiningIntensity(Poco::UInt32 intensity)
{
	Poco::Mutex::ScopedLock lock(worker_mutex_);

	// dont change it if its the same intensity
	// changing it is a heavy task
	if (MinerConfig::getConfig().getMiningIntensity() == intensity)
		return;

	shut_down_worker(*verifier_pool_, *verifier_, verificationQueue_);
	MinerConfig::getConfig().setMininigIntensity(intensity);
	MinerHelper::create_worker<PlotVerifier>(verifier_pool_, verifier_, MinerConfig::getConfig().getMiningIntensity(),
		*this, verificationQueue_);
}

void Burst::Miner::setMaxPlotReader(Poco::UInt32 max_reader)
{
	Poco::Mutex::ScopedLock lock(worker_mutex_);

	// dont change it if its the same reader count
	// changing it is a heavy task
	if (MinerConfig::getConfig().getMaxPlotReaders() == max_reader)
		return;

	shut_down_worker(*plot_reader_pool_, *plot_reader_, plotReadQueue_);
	MinerConfig::getConfig().setMaxPlotReaders(max_reader);
	MinerHelper::create_worker<PlotReader>(plot_reader_pool_, plot_reader_, MinerConfig::getConfig().getMaxPlotReaders(),
		*this, progress_, verificationQueue_, plotReadQueue_);
}

void Burst::Miner::setMaxBufferSize(Poco::UInt64 size)
{
	MinerConfig::getConfig().setBufferSize(size);
	PlotReader::globalBufferSize.setMax(size);
}
