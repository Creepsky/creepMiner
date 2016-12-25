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
#include "NonceSubmitter.hpp"
#include "rapidjson/document.h"
#include <Poco/URI.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>

Burst::Miner::~Miner()
{}

void Burst::Miner::run()
{
	running_ = true;
	progress_ = std::make_shared<PlotReadProgress>();

	auto& config = MinerConfig::getConfig();
	auto errors = 0u;

	if (config.getPoolUrl().empty() ||
		config.getMiningInfoUrl().empty())
	{
		MinerLogger::write("Mining networking failed", TextType::Error);
		return;
	}

	//MinerLogger::write("Submission Max Delay : " + std::to_string(config.submissionMaxDelay), TextType::System);
	MinerLogger::write("Submission Max Retry : " + std::to_string(config.getSubmissionMaxRetry()), TextType::System);
	MinerLogger::write("Receive Max Retry : " + std::to_string(config.getReceiveMaxRetry()), TextType::System);
	MinerLogger::write("Send Max Retry : " + std::to_string(config.getSendMaxRetry()), TextType::System);
	MinerLogger::write("Send-Timeout : " + std::to_string(config.getSendTimeout()) + " seconds", TextType::System);
	MinerLogger::write("Receive-Timeout : " + std::to_string(config.getReceiveTimeout()) + " seconds", TextType::System);
	MinerLogger::write("Buffer Size : " + std::to_string(config.maxBufferSizeMB) + " MB", TextType::System);
	MinerLogger::write("Pool Host : " + config.getPoolUrl().getCanonical() + ":" + std::to_string(config.getPoolUrl().getPort()) +
		" (" + config.getPoolUrl().getIp() + ")", TextType::System);
	MinerLogger::write("Mininginfo URL : " + config.getMiningInfoUrl().getCanonical() + ":" + std::to_string(config.getMiningInfoUrl().getPort()) +
		" (" + config.getMiningInfoUrl().getIp() + ")", TextType::System);
	if (!config.getWalletUrl().getCanonical().empty())
		MinerLogger::write("Wallet URL : " + config.getWalletUrl().getCanonical() + ":" + std::to_string(config.getWalletUrl().getPort()) +
			" (" + config.getWalletUrl().getIp() + ")", TextType::System);

	miningInfoSession_ = MinerConfig::getConfig().createSession(HostType::MiningInfo);
	miningInfoSession_->setKeepAlive(true);

	wallet_ = MinerConfig::getConfig().getWalletUrl();

	const auto sleepTime = std::chrono::seconds(3);
	running_ = true;

	wallet_.getLastBlock(blockHeight_);

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
	this->running_ = false;
}

void Burst::Miner::updateGensig(const std::string gensigStr, uint64_t blockHeight, uint64_t baseTarget)
{
	// why we start a new thread to gather the last winner:
	// it could be slow and is not necessary for the whole process
	// so show it when it's done
	if (blockHeight_ > 0 && !MinerConfig::getConfig().getWalletUrl().getIp().empty())
	{
		// copy blockheight temporary for lambda arg capture
		auto block = blockHeight_;

		std::thread threadLastWinner([this, block]()
		{
			AccountId lastWinner;

			if (wallet_.getWinnerOfBlock(block, lastWinner))
			{
				std::string name;
				wallet_.getNameOfAccount(lastWinner, name);

				// if we dont fetched the winner of the last block
				// we exit this thread function
				if (blockHeight_ - 1 != block)
					return;

				MinerLogger::write(std::string(50, '-'), TextType::Ok);
				MinerLogger::write("last block winner: ", TextType::Ok);
				MinerLogger::write("block#             " + std::to_string(block), TextType::Ok);
				MinerLogger::write("winner-numeric     " + std::to_string(lastWinner), TextType::Ok);
				MinerLogger::write("winner-address     " + NxtAddress(lastWinner).to_string(), TextType::Ok);
				if (!name.empty())
					MinerLogger::write("winner-name        " + name, TextType::Ok);
				MinerLogger::write(std::string(50, '-'), TextType::Ok);
			}
		});
		
		threadLastWinner.detach();
	}

	// setup new block-data
	gensigStr_ = gensigStr;
	blockHeight_ = blockHeight;
	baseTarget_ = baseTarget;

	MinerLogger::write("stopping plot readers...", TextType::Debug);

	// stop all reading processes if any
	for (auto& plotReader : plotReaders_)
		plotReader->stop();

	MinerLogger::write("waiting plot readers to stop...", TextType::Debug);

	// wait for all plotReaders to stop
	auto stopped = false;
	//
	while (!stopped)
	{
		stopped = true;

		for (auto& plotReader : plotReaders_)
		{
			if (!plotReader->isDone())
			{
				stopped = false;
				break;
			}
		}
	}

	MinerLogger::write("plot readers stopped", TextType::Debug);
	std::lock_guard<std::mutex> lock(deadlinesLock_);
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
	scoopNum_ = (static_cast<int>(newGenSig[newGenSig.size() - 2] & 0x0F) << 8) | static_cast<int>(newGenSig[newGenSig.size() - 1]);

	MinerLogger::write(std::string(50, '-'), TextType::Information);
	MinerLogger::write("block#      " + std::to_string(blockHeight), TextType::Information);
	MinerLogger::write("scoop#      " + std::to_string(scoopNum_), TextType::Information);
	MinerLogger::write("baseTarget# " + std::to_string(baseTarget), TextType::Information);
	//MinerLogger::write("gensig#     " + gensigStr, TextType::Information);
	MinerLogger::write(std::string(50, '-'), TextType::Information);

	// this block is closed in itself
	// dont use the variables in it outside!
	{
		using PlotList = std::vector<std::shared_ptr<PlotFile>>;
		std::unordered_map<std::string, PlotList> plotDirs;

		for (const auto plotFile : MinerConfig::getConfig().getPlotFiles())
		{
			auto last_slash_idx = plotFile->getPath().find_last_of("/\\");

			std::string dir;

			if (last_slash_idx == std::string::npos)
				continue;

			dir = plotFile->getPath().substr(0, last_slash_idx);

			auto iter = plotDirs.find(dir);

			if (iter == plotDirs.end())
				plotDirs.emplace(std::make_pair(dir, PlotList {}));

			plotDirs[dir].emplace_back(plotFile);
		}

		progress_->reset();
		progress_->setMax(MinerConfig::getConfig().getTotalPlotsize());

		for (auto& plotDir : plotDirs)
		{
			auto reader = std::make_shared<PlotListReader>(*this, progress_);
			reader->read(std::string(plotDir.first), std::move(plotDir.second));
			plotReaders_.emplace_back(reader);
		}
	}
}

const Burst::GensigData& Burst::Miner::getGensig() const
{
	return this->gensig_;
}

uint64_t Burst::Miner::getBaseTarget() const
{
	return this->baseTarget_;
}

uint64_t Burst::Miner::getBlockheight() const
{
	return blockHeight_;
}

uint64_t Burst::Miner::getTargetDeadline() const
{
	return targetDeadline_;
}

size_t Burst::Miner::getScoopNum() const
{
	return this->scoopNum_;
}

void Burst::Miner::submitNonce(uint64_t nonce, uint64_t accountId, uint64_t deadline, std::string plotFile)
{
	std::lock_guard<std::mutex> mutex(deadlinesLock_);

	auto bestDeadline = deadlines_[accountId].getBest();

	// is the new nonce better then the best one we already have?
	if (bestDeadline == nullptr || bestDeadline->getDeadline() > deadline)
	{
		auto newDeadline = deadlines_[accountId].add({ nonce, deadline, accountId, blockHeight_, plotFile,
			accountNames_.getName(accountId, wallet_) });

		if (MinerConfig::getConfig().output.nonceFound)
		{
			auto message = newDeadline->getAccountName() + ": nonce found (" + Burst::deadlineFormat(deadline) + ")";
			
			if (MinerConfig::getConfig().output.nonceFoundPlot)
				message += " in " + plotFile;

			MinerLogger::write(message, TextType::Unimportant);
		}

		auto createSendThread = true;

		if (getTargetDeadline() > 0 && deadline >= getTargetDeadline())
		{
			createSendThread = false;

			MinerLogger::write("Nonce is higher then the target deadline of the pool (" +
				deadlineFormat(getTargetDeadline()) + ")", TextType::Debug);
		}

		newDeadline->send();

		if (createSendThread)
			NonceSubmitter{*this, newDeadline }.startSubmit();
	}
}

void Burst::Miner::nonceSubmitReport(uint64_t nonce, uint64_t accountId, uint64_t deadline)
{
	std::lock_guard<std::mutex> mutex(deadlinesLock_);

	if (deadlines_[accountId].confirm(nonce, accountId, blockHeight_))
		if (deadlines_[accountId].getBestConfirmed()->getDeadline() == deadline)
			MinerLogger::write(NxtAddress(accountId).to_string() + ": nonce confirmed (" + deadlineFormat(deadline) + ")", TextType::Success);
}

bool Burst::Miner::getMiningInfo()
{
	using namespace Poco::Net;
	
	Request request(std::move(miningInfoSession_));

	HTTPRequest requestData{HTTPRequest::HTTP_GET, "/burst?requestType=getMiningInfo", HTTPRequest::HTTP_1_1};
	requestData.setKeepAlive(true);

	auto response = request.send(requestData);
	std::string responseData;

	if (response.receive(responseData))
	{
		HttpResponse httpResponse(responseData);
		rapidjson::Document body;

		body.Parse<0>(httpResponse.getMessage().c_str());	

		if (body.GetParseError() == nullptr)
		{
			std::string gensig;

			if (body.HasMember("baseTarget"))
			{
				std::string baseTargetStr = body["baseTarget"].GetString();
				currentBaseTarget_ = std::stoull(baseTargetStr);
			}

			if (body.HasMember("generationSignature"))
			{
				gensig = body["generationSignature"].GetString();
			}

			if (body.HasMember("targetDeadline"))
			{
				targetDeadline_ = body["targetDeadline"].GetUint64();
			}

			if (body.HasMember("height"))
			{
				std::string newBlockHeightStr = body["height"].GetString();
				auto newBlockHeight = std::stoull(newBlockHeightStr);

				if (newBlockHeight > currentBlockHeight_)
					updateGensig(gensig, newBlockHeight, currentBaseTarget_);

				currentBlockHeight_ = newBlockHeight;
			}

			transferSession(response, miningInfoSession_);
			return true;
		}

		MinerLogger::write("Error on getting new block-info!", TextType::Error);
		MinerLogger::write(body.GetParseError(), TextType::Error);
		MinerLogger::write("Full response:", TextType::Error);
		MinerLogger::write(httpResponse.getResponse(), TextType::Error);
	}
	
	transferSession(request, miningInfoSession_);
	transferSession(response, miningInfoSession_);

	return false;
}

std::shared_ptr<Burst::Deadline> Burst::Miner::getBestSent(uint64_t accountId, uint64_t blockHeight)
{
	std::lock_guard<std::mutex> mutex(deadlinesLock_);

	if (blockHeight != this->blockHeight_)
		return nullptr;

	return deadlines_[accountId].getBestSent();
}
