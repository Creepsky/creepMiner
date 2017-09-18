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

#include "Miner.hpp"
#include "logging/MinerLogger.hpp"
#include "MinerConfig.hpp"
#include "plots/PlotReader.hpp"
#include "MinerUtil.hpp"
#include "network/Request.hpp"
#include <Poco/Net/HTTPRequest.h>
#include "network/NonceSubmitter.hpp"
#include <Poco/JSON/Parser.h>
#include "plots/PlotSizes.hpp"
#include "logging/Performance.hpp"
#include <Poco/FileStream.h>
#include <fstream>
#include <Poco/File.h>
#include <Poco/Delegate.h>
#include "plots/PlotVerifier.hpp"
#include "MinerCL.hpp"

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

Burst::Miner::~Miner() = default;

void Burst::Miner::run()
{
	poco_ndc(Miner::run);
	running_ = true;
	progressRead_ = std::make_shared<PlotReadProgress>();
	progressVerify_ = std::make_shared<PlotReadProgress>();

	progressRead_->progressChanged.add(Poco::delegate(this, &Miner::progressChanged));
	progressVerify_->progressChanged.add(Poco::delegate(this, &Miner::progressChanged));

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
			*this, progressRead_, verificationQueue_, plotReadQueue_);

		// create the plot verifiers
		createPlotVerifiers();

#ifndef USE_CUDA
		if (config.getProcessorType() == "CUDA")
			log_error(MinerLogger::miner, "You are mining with your CUDA GPU, but the miner is compiled without the CUDA SDK!\n"
				"You will not see any deadline coming from this miner!");
#endif
	}

	wallet_ = MinerConfig::getConfig().getWalletUrl();

	const auto wakeUpTime = static_cast<long>(config.getWakeUpTime());

	if (wakeUpTime > 0)
	{
		const Poco::TimerCallback<Miner> callback(*this, &Miner::on_wake_up);

		wake_up_timer_.setPeriodicInterval(wakeUpTime * 1000);
		wake_up_timer_.start(callback);
	}

	const auto benchmark = MinerConfig::getConfig().isBenchmark();
	const auto benchmarkInterval = MinerConfig::getConfig().getBenchmarkInterval();

	if (benchmark)
	{
		const Poco::TimerCallback<Miner> callback(*this, &Miner::onBenchmark);
		benchmark_timer_.setPeriodicInterval(benchmarkInterval * 1000u);
		benchmark_timer_.start(callback);
	}

	running_ = true;

	miningInfoSession_ = MinerConfig::getConfig().createSession(HostType::MiningInfo);
	miningInfoSession_->setKeepAlive(true);

	// TODO REWORK
	//wallet_.getLastBlock(currentBlockHeight_);

	log_information(MinerLogger::miner, "Looking for mining info...");

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

		std::this_thread::sleep_for(std::chrono::seconds(MinerConfig::getConfig().getMiningInfoInterval()));
	}

	if (wakeUpTime > 0)
		wake_up_timer_.stop();

	running_ = false;
}

void Burst::Miner::stop()
{
	poco_ndc(Miner::stop);
	// stop plot reader
	shut_down_worker(*plot_reader_pool_, *plot_reader_, plotReadQueue_);
	// stop verifier
	shut_down_worker(*verifier_pool_, *verifier_, verificationQueue_);
	
	running_ = false;
}

void Burst::Miner::addPlotReadNotifications(bool wakeUpCall)
{
	const auto initPlotReadNotification = [this, wakeUpCall](PlotDir& plotDir)
	{
		auto notification = new PlotReadNotification;
		notification->dir = plotDir.getPath();
		notification->gensig = getGensig();
		notification->scoopNum = getScoopNum();
		notification->blockheight = getBlockheight();
		notification->baseTarget = getBaseTarget();
		notification->type = plotDir.getType();
		notification->wakeUpCall = wakeUpCall;

		for (const auto& plotFile : plotDir.getPlotfiles(true))
			accounts_.getAccount(plotFile->getAccountId(), wallet_, true);

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

void Burst::Miner::updateGensig(const std::string gensigStr, Poco::UInt64 blockHeight, Poco::UInt64 baseTarget)
{
	poco_ndc(Miner::updateGensig);

	CLEAR_PROBES()
	START_PROBE("Miner.StartNewBlock")

	// stop all reading processes if any
	if (!MinerConfig::getConfig().getPlotFiles().empty())
	{
		log_debug(MinerLogger::miner, "Plot-read-queue: %d, verification-queue: %d",
			plotReadQueue_.size(), verificationQueue_.size());
		log_debug(MinerLogger::miner, "Allocated memory: %s", memToString(PlotReader::globalBufferSize.getSize(), 1));
	
		START_PROBE("Miner.SetBuffersize")
		PlotReader::globalBufferSize.setMax(MinerConfig::getConfig().getMaxBufferSize() * 1024 * 1024);
		TAKE_PROBE("Miner.SetBuffersize")
	}
	
	// clear the plot read queue
	plotReadQueue_.clear();

	// setup new block-data
	auto block = data_.startNewBlock(blockHeight, baseTarget, gensigStr);

	// printing block info and transfer it to local server
	{
		log_notice(MinerLogger::miner, std::string(50, '-') + "\n"
			"block#     \t%Lu\n"
			"scoop#     \t%Lu\n"
			"baseTarget#\t%Lu\n"
			"gensig     \t%s\n" +
			std::string(50, '-'),
			blockHeight, block->getScoop(), baseTarget, createTruncatedString(getGensigStr(), 14, 32)
		);

		data_.getBlockData()->refreshBlockEntry();
	}

	if (MinerConfig::getConfig().isRescanningEveryBlock())
		MinerConfig::getConfig().rescanPlotfiles();

	progressRead_->reset(blockHeight, MinerConfig::getConfig().getTotalPlotsize());
	progressVerify_->reset(blockHeight, MinerConfig::getConfig().getTotalPlotsize());

	PlotSizes::nextRound();
	PlotSizes::refresh(MinerConfig::getConfig().getPlotsHash());
	startPoint_ = std::chrono::high_resolution_clock::now();

	addPlotReadNotifications();

	data_.getWonBlocksAsync(wallet_, accounts_);

	// why we start a new thread to gather the last winner:
	// it could be slow and is not necessary for the whole process
	// so show it when it's done
	if (blockHeight > 0 && wallet_.isActive())
		block->getLastWinnerAsync(wallet_, accounts_);

	TAKE_PROBE("Miner.StartNewBlock")
}

const Burst::GensigData& Burst::Miner::getGensig() const
{
	const auto blockData = data_.getBlockData();

	if (blockData == nullptr)
	{
		log_current_stackframe(MinerLogger::miner);
		throw Poco::NullPointerException{"Tried to access the gensig from a nullptr!"};
	}

	return blockData->getGensig();
}

const std::string& Burst::Miner::getGensigStr() const
{
	const auto blockData = data_.getBlockData();

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

	const auto targetDeadline = getTargetDeadline();
	auto tooHigh = targetDeadline > 0 && deadline > targetDeadline;

	if (tooHigh && !MinerLogger::hasOutput(NonceFoundTooHigh))
		return SubmitResponse::TooHigh;

	auto block = data_.getBlockData();

	if (block == nullptr)
		return SubmitResponse::Error;

	//auto bestDeadline = block->getBestDeadline(accountId, BlockData::DeadlineSearchType::Found);

	newDeadline = block->addDeadlineIfBest(
		nonce,
		deadline,
		accounts_.getAccount(accountId, wallet_, true),
		data_.getBlockData()->getBlockheight(),
		plotFile
	);

	if (newDeadline)
	{
		log_unimportant_if(MinerLogger::miner, MinerLogger::hasOutput(NonceFound) || tooHigh,
			"%s: nonce found (%s)\n"
			"\tnonce: %Lu\n"
			"\tin:    %s",
			newDeadline->getAccountName(), deadlineFormat(deadline), newDeadline->getNonce(), plotFile);
			
		if (tooHigh)
			return SubmitResponse::TooHigh;

		return SubmitResponse::Found;
	}

	return SubmitResponse::Error;
}

void Burst::Miner::submitNonce(Poco::UInt64 nonce, Poco::UInt64 accountId, Poco::UInt64 deadline, Poco::UInt64 blockheight, std::string plotFile)
{
	poco_ndc(Miner::submitNonce);
	
	std::shared_ptr<Deadline> newDeadline;

	const auto result = addNewDeadline(nonce, accountId, deadline, blockheight, std::move(plotFile), newDeadline);

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
			if (responseData.empty())
				return false;

			HttpResponse httpResponse(responseData);
			Poco::JSON::Parser parser;
			Poco::JSON::Object::Ptr root;

			try
			{
				root = parser.parse(httpResponse.getMessage()).extract<Poco::JSON::Object::Ptr>();
			}
			catch (...)
			{
				return false;
			}

			std::string gensig;

			if (root->has("height"))
			{
				const std::string newBlockHeightStr = root->get("height");
				const auto newBlockHeight = std::stoull(newBlockHeightStr);

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
								"got new target deadline from pool\n"
								"\told pool target deadline: %s\n"
								"\tnew pool target deadline: %s\n"
								"\ttarget deadline from config: %s\n"
								"\tlowest target deadline: %s",
								deadlineFormat(target_deadline_pool_before),
								deadlineFormat(data_.getTargetDeadline(TargetDeadlineType::Pool)),
								deadlineFormat(data_.getTargetDeadline(TargetDeadlineType::Local)),
								deadlineFormat(data_.getTargetDeadline()))


;
					}

					updateGensig(gensig, newBlockHeight, std::stoull(baseTargetStr));
				}
			}

			transferSession(response, miningInfoSession_);
			return true;
		}
		catch (Poco::Exception& exc)
		{
			log_error(MinerLogger::miner, "Error on getting new block-info!\n\t%s", exc.displayText());
			// because the full response may be too long, we only log the it in the logfile
			log_file_only(MinerLogger::miner, Poco::Message::PRIO_ERROR, TextType::Error, "Block-info full response:\n%s", responseData);
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

namespace Burst
{
	std::mutex progressMutex_;

	void showProgress(PlotReadProgress& progressRead, PlotReadProgress& progressVerify, MinerData& data, Poco::UInt64 blockheight,
		std::chrono::high_resolution_clock::time_point& startPoint)
	{
		std::lock_guard<std::mutex> lock(progressMutex_);

		const auto readProgressPercent = progressRead.getProgress();
		const auto verifyProgressPercent = progressVerify.getProgress();
		const auto readProgressValue = progressRead.getValue();
		const auto verifyProgressValue = progressVerify.getValue();

		const auto timeDiff = std::chrono::high_resolution_clock::now() - startPoint;
		const auto timeDiffSeconds = std::chrono::duration<double>(timeDiff);
		
		Progress progress;
		progress.read = readProgressPercent;
		progress.verify = verifyProgressPercent;
		progress.bytesPerSecondRead = readProgressValue / 4096 / timeDiffSeconds.count();
		progress.bytesPerSecondVerify = verifyProgressValue / 4096 / timeDiffSeconds.count();
		progress.bytesPerSecondCombined = (readProgressValue + verifyProgressValue) / 4096 / timeDiffSeconds.
			count();

		MinerLogger::writeProgress(progress);
		data.getBlockData()->setProgress(readProgressPercent, verifyProgressPercent, blockheight);
	}
}

void Burst::Miner::progressChanged(float &progress)
{
	showProgress(*progressRead_, *progressVerify_, getData(), getBlockheight(), startPoint_);
}

void Burst::Miner::on_wake_up(Poco::Timer& timer)
{
	addPlotReadNotifications(true);
}

void Burst::Miner::onBenchmark(Poco::Timer& timer)
{
	try
	{
		Poco::FileStream perfLogStream{ "benchmark.csv", std::ios_base::out | std::ios::trunc };
		perfLogStream << Performance::instance();
		log_success(MinerLogger::miner, "Wrote benchmark data into benchmark.csv");
	}
	catch (...)
	{
		log_error(MinerLogger::miner, "Could not write benchmark data into benchmark.csv! Please close it.");
	}
}

Burst::NonceConfirmation Burst::Miner::submitNonceAsyncImpl(const std::tuple<Poco::UInt64, Poco::UInt64, Poco::UInt64, Poco::UInt64, std::string>& data)
{
	poco_ndc(Miner::submitNonceImpl);

	const auto nonce = std::get<0>(data);
	const auto accountId = std::get<1>(data);
	const auto deadline = std::get<2>(data);
	const auto blockheight = std::get<3>(data);
	const auto& plotFile = std::get<4>(data);

	std::shared_ptr<Deadline> newDeadline;

	const auto result = addNewDeadline(nonce, accountId, deadline, blockheight, plotFile, newDeadline);

	// is the new nonce better then the best one we already have?
	if (result == SubmitResponse::Found)
	{
		newDeadline->onTheWay();
		return NonceSubmitter{ *this, newDeadline }.submit();
	}

	NonceConfirmation nonceConfirmation;
	nonceConfirmation.deadline = 0;
	nonceConfirmation.json = Poco::format(R"({ "result" : "success", "deadline" : %Lu })", deadline);
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

void Burst::Miner::createPlotVerifiers()
{
	const auto& config = MinerConfig::getConfig();
	// if true, SSE2 is chosen instead of whatever the user has in his config
	// this is mostly done as a fallback strategy, because most CPUs have SSE2
	auto forceCpu = false;

	const auto& processorType = config.getProcessorType();
	const auto& cpuInstructionSet = config.getCpuInstructionSet();
	auto forceSse2 = false;

	if (processorType == "CPU" && !forceSse2)
	{
		if (cpuInstructionSet == "SSE4" && Settings::Sse4)
			MinerHelper::create_worker<PlotVerifier_sse4>(verifier_pool_, verifier_, MinerConfig::getConfig().getMiningIntensity(),
				*this, verificationQueue_, progressVerify_);
		else if (cpuInstructionSet == "AVX" && Settings::Avx)
			MinerHelper::create_worker<PlotVerifier_avx>(verifier_pool_, verifier_, MinerConfig::getConfig().getMiningIntensity(),
				*this, verificationQueue_, progressVerify_);
		else if (cpuInstructionSet == "AVX2" && Settings::Avx2)
			MinerHelper::create_worker<PlotVerifier_avx2>(verifier_pool_, verifier_, MinerConfig::getConfig().getMiningIntensity(),
				*this, verificationQueue_, progressVerify_);
		else
			forceSse2 = true;
	}
	else if (processorType == "CUDA" && Settings::Cuda)
	{
		MinerHelper::create_worker<PlotVerifier_cuda>(verifier_pool_, verifier_, MinerConfig::getConfig().getMiningIntensity(),
			*this, verificationQueue_, progressVerify_);
	}
	else if (config.getProcessorType() == "OPENCL")
	{
		MinerHelper::create_worker<PlotVerifier_opencl>(verifier_pool_, verifier_, MinerConfig::getConfig().getMiningIntensity(),
			*this, verificationQueue_, progressVerify_);
	}
	else
		forceSse2 = true;
	
	if (forceSse2)
	{
		MinerHelper::create_worker<PlotVerifier_sse2>(verifier_pool_, verifier_, MinerConfig::getConfig().getMiningIntensity(),
			*this, verificationQueue_, progressVerify_);
	
		if (forceSse2)
			log_warning(MinerLogger::miner, "You are using the processor type %s with the instruction set %s,\n"
				"but your miner is compiled without that feature!\n"
				"As a fallback solution CPU with SSE2 is used.", processorType, cpuInstructionSet);
	}
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
	createPlotVerifiers();
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
		*this, progressRead_, verificationQueue_, plotReadQueue_);
}

void Burst::Miner::setMaxBufferSize(Poco::UInt64 size)
{
	MinerConfig::getConfig().setBufferSize(size);
	PlotReader::globalBufferSize.setMax(size);
}

void Burst::Miner::rescanPlotfiles()
{
	// rescan the plot files...
	if (MinerConfig::getConfig().rescanPlotfiles())
	{
		// we send the new settings (size could be changed)
		data_.getBlockData()->refreshConfig();

		// then we send all plot dirs and files
		data_.getBlockData()->refreshPlotDirs();
	}
}
