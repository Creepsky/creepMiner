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
#include <Poco/Path.h>
#include "NonceSubmitter.hpp"
#include <algorithm>
#include <Poco/JSON/Parser.h>
#include <Poco/NestedDiagnosticContext.h>

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

	if (config.getPoolUrl().empty() ||
		config.getMiningInfoUrl().empty())
	{
		std::vector<std::string> lines = {
			"Pool URL and/or mininginfo URL is empty!",
			"Pool URL: '" + config.getPoolUrl().getCanonical(false) + "'",
			"Mininginfo URL: '" + config.getMiningInfoUrl().getCanonical(false) + "'"
		};
		
		MinerLogger::write(lines, TextType::Error);
		return;
	}

	MinerLogger::write("Total plots size: " + memToString(MinerConfig::getConfig().getTotalPlotsize(), 2),
					   TextType::System);
	MinerLogger::write("Submission Max Retry : " + std::to_string(config.getSubmissionMaxRetry()), TextType::System);
	MinerLogger::write("Buffer Size : " + std::to_string(config.maxBufferSizeMB) + " MB", TextType::System);
	MinerLogger::write("Pool Host : " + config.getPoolUrl().getCanonical() + ":" + std::to_string(config.getPoolUrl().getPort()) +
		" (" + config.getPoolUrl().getIp() + ")", TextType::System);
	MinerLogger::write("Mininginfo URL : " + config.getMiningInfoUrl().getCanonical() + ":" + std::to_string(config.getMiningInfoUrl().getPort()) +
		" (" + config.getMiningInfoUrl().getIp() + ")", TextType::System);
	if (!config.getWalletUrl().empty())
		MinerLogger::write("Wallet URL : " + config.getWalletUrl().getCanonical() + ":" + std::to_string(config.getWalletUrl().getPort()) +
			" (" + config.getWalletUrl().getIp() + ")", TextType::System);
	if (config.getStartServer() && !config.getServerUrl().empty())
		MinerLogger::write("Server URL : " + config.getServerUrl().getCanonical() + ":" + std::to_string(config.getServerUrl().getPort()) +
			" (" + config.getServerUrl().getIp() + ")", TextType::System);
	if (config.getTargetDeadline() > 0)
		MinerLogger::write("Target deadline : " + deadlineFormat(config.getTargetDeadline()), TextType::System);
	MinerLogger::write("Mining intensity : " + std::to_string(config.getMiningIntensity()), TextType::System);
	
	auto plotFileSize = static_cast<int32_t>(config.getPlotFiles().size());
	
	if (plotFileSize <= 0)
		plotFileSize = 1;

	plotReaderThreadPool_ = std::make_unique<Poco::ThreadPool>(plotFileSize, plotFileSize);
	plotReaderManager_ = std::make_unique<Poco::TaskManager>(*plotReaderThreadPool_);

	nonceSubmitterThreadPool_ = std::make_unique<Poco::ThreadPool>();
	nonceSubmitterManager_ = std::make_unique<Poco::TaskManager>(*nonceSubmitterThreadPool_);

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
			++errors;
		
		// we have a tollerance of 5 times of not being able to fetch mining infos, before its a real error
		if (errors >= 5)
		{
			// reset error-counter and show error-message in console
			MinerLogger::write("Could not get block infos!", TextType::Error);
			errors = 0;
		}

		std::this_thread::sleep_for(sleepTime);
	}

	running_ = false;
}

void Burst::Miner::stop()
{
	poco_ndc(Miner::stop);
	plotReaderManager_->cancelAll();
	plotReaderManager_->joinAll();
	nonceSubmitterManager_->cancelAll();
	nonceSubmitterManager_->joinAll();
	this->running_ = false;
}

void Burst::Miner::updateGensig(const std::string gensigStr, uint64_t blockHeight, uint64_t baseTarget)
{
	poco_ndc(Miner::updateGensig);
	
	// stop all reading processes if any
	MinerLogger::write("stopping plot readers...", TextType::Debug);
	plotReaderManager_->cancelAll();

	// wait for all plotReaders to stop
	MinerLogger::write("waiting plot readers to stop...", TextType::Debug);
	plotReaderManager_->joinAll();
	MinerLogger::write("plot readers stopped", TextType::Debug);

	Poco::ScopedLock<Poco::FastMutex> lock{ deadlinesLock_ };
	deadlines_.clear();

	for (auto i = 0; i < 32; ++i)
	{
		auto byteStr = gensigStr.substr(i * 2, 2);
		this->gensig_[i] = static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16));
	}

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
		!MinerConfig::getConfig().getWalletUrl().getIp().empty() &&
		MinerConfig::getConfig().output.lastWinner)
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
				!fetched && loop < MinerConfig::getConfig().getSubmissionMaxRetry();
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

					std::vector<std::string> lines = {
						std::string(50, '-'),
						"last block winner: ",
						"block#             " + std::to_string(block),
						"winner-numeric     " + std::to_string(lastWinner),
						"winner-address     " + address
					};

					auto lastWinnerPtr = std::make_shared<Poco::JSON::Object>();
					lastWinnerPtr->set("type", "lastWinner");
					lastWinnerPtr->set("numeric", lastWinner);
					lastWinnerPtr->set("address", address);

					if (!name.empty())
					{
						lines.emplace_back("winner-name        " + name);
						lastWinnerPtr->set("name", name);
					}
					
					lines.emplace_back(std::string(50, '-'));

					MinerLogger::write(lines, TextType::Ok);
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
		auto lines = {
			std::string(50, '-'),
			std::string("block#      " + std::to_string(blockHeight)),
			std::string("scoop#      " + std::to_string(scoopNum)),
			std::string("baseTarget# " + std::to_string(baseTarget)),
			//std::string("gensig#     " + gensigStr),
			std::string(50, '-')
		};

		MinerLogger::write(lines, TextType::Information);
		
		data_.addBlockEntry(createJsonNewBlock(data_));
	}
	
	progress_->reset();
	progress_->setMax(MinerConfig::getConfig().getTotalPlotsize());

	for (auto& plotDir : MinerConfig::getConfig().getPlotList())
		plotReaderManager_->start(new PlotListReader{*this, progress_,
			std::string(plotDir.first), plotDir.second});
}

const Burst::GensigData& Burst::Miner::getGensig() const
{
	return this->gensig_;
}

uint64_t Burst::Miner::getBaseTarget() const
{
	return data_.getBlockData()->baseTarget;
}

uint64_t Burst::Miner::getBlockheight() const
{
	return data_.getCurrentBlock();
}

uint64_t Burst::Miner::getTargetDeadline() const
{
	return targetDeadline_;
}

size_t Burst::Miner::getScoopNum() const
{
	return data_.getBlockData()->scoop;
}

void Burst::Miner::submitNonce(uint64_t nonce, uint64_t accountId, uint64_t deadline, std::string plotFile)
{
	poco_ndc(Miner::submitNonce);
	Poco::ScopedLock<Poco::FastMutex> mutex{ deadlinesLock_ };

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

		if (MinerConfig::getConfig().output.nonceFound)
		{
			auto message = newDeadline->getAccountName() + ": nonce found (" + Burst::deadlineFormat(deadline) + ")";
			
			if (MinerConfig::getConfig().output.nonceFoundPlot)
				message += " in " + plotFile;
			
			data_.addBlockEntry(createJsonDeadline(newDeadline, "nonce found"));

			MinerLogger::write(message, TextType::Unimportant);
		}

		auto createSendThread = true;
		auto targetDeadline = getTargetDeadline();
		auto manualTargetDeadline = MinerConfig::getConfig().getTargetDeadline();

		if (manualTargetDeadline > 0)
			targetDeadline = targetDeadline < manualTargetDeadline ? targetDeadline : manualTargetDeadline;

		if (targetDeadline > 0 && newDeadline->getDeadline() >= targetDeadline)
		{
			createSendThread = false;

			MinerLogger::write("Nonce is higher then the target deadline of the pool (" +
				deadlineFormat(targetDeadline) + ")", TextType::Debug);
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

	HTTPRequest requestData{HTTPRequest::HTTP_GET, "/burst?requestType=getMiningInfo", HTTPRequest::HTTP_1_1};
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
			
			MinerLogger::writeStackframe(lines);
		}
	}
	
	transferSession(request, miningInfoSession_);
	transferSession(response, miningInfoSession_);

	return false;
}

std::shared_ptr<Burst::Deadline> Burst::Miner::getBestSent(uint64_t accountId, uint64_t blockHeight)
{
	poco_ndc(Miner::getBestSent);
	Poco::ScopedLock<Poco::FastMutex> mutex{ deadlinesLock_ };

	if (blockHeight != data_.getCurrentBlock())
		return nullptr;

	return deadlines_[accountId].getBestSent();
}

std::shared_ptr<Burst::Deadline> Burst::Miner::getBestConfirmed(uint64_t accountId, uint64_t blockHeight)
{
	poco_ndc(Miner::getBestConfirmed);
	Poco::ScopedLock<Poco::FastMutex> mutex{ deadlinesLock_ };

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
		return{};

	entries.reserve(blockData->entries.size());

	for (auto& entry : blockData->entries)
		entries.emplace_back(entry);

	return entries;
}

Burst::MinerData& Burst::Miner::getData()
{
	return data_;
}
