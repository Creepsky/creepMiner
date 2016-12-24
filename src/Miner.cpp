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
#include <algorithm>
#include "Response.hpp"
#include "Request.hpp"
#include "NonceSubmitter.hpp"
#include "rapidjson/document.h"
#include <Poco/URI.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPRequest.h>

Burst::Miner::~Miner()
{}

void Burst::Miner::run()
{
	running_ = true;
	std::thread submitter(&Miner::nonceSubmitterThread, this);

	progress_ = std::make_shared<PlotReadProgress>();

	//sockets_ = PoolSockets{5};
	//sockets_.fill();

	auto& config = MinerConfig::getConfig();
	auto errors = 0u;

	if (config.urlPool.getIp().empty() ||
		config.urlMiningInfo.getIp().empty())
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
	MinerLogger::write("Pool Host : " + config.urlPool.getCanonical() + ":" + std::to_string(config.urlPool.getPort()) +
		" (" + config.urlPool.getIp() + ")", TextType::System);
	MinerLogger::write("Mininginfo URL : " + config.urlMiningInfo.getCanonical() + ":" + std::to_string(config.urlMiningInfo.getPort()) +
		" (" + config.urlMiningInfo.getIp() + ")", TextType::System);

	miningInfoSession_ = MinerConfig::getConfig().createSession(HostType::MiningInfo);
	miningInfoSession_->setKeepAlive(true);

	const auto sleepTime = std::chrono::seconds(3);
	running_ = true;

	while (running_)
	{
		std::string data;

		if (getMiningInfo())
			errors = 0;
		else
			++errors;

		// we have a tollerance of 3 times of not being able to fetch mining infos, before its a real error
		if (errors >= 3)
		{
			// reset error-counter and show error-message in console
			MinerLogger::write("Could not get block infos!", TextType::Error);
			errors = 0;
		}

		std::this_thread::sleep_for(sleepTime);
	}

	running_ = false;
	submitter.join();
}

void Burst::Miner::stop()
{
	this->running_ = false;
}

void Burst::Miner::updateGensig(const std::string gensigStr, uint64_t blockHeight, uint64_t baseTarget)
{
	// setup new block-data
	this->gensigStr_ = gensigStr;
	this->blockHeight_ = blockHeight;
	this->baseTarget_ = baseTarget;

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

	submitThreadNotified_ = true;
	newBlockIncoming_.notify_one();
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

void Burst::Miner::nonceSubmitterThread()
{
	while (running_)
	{
		std::unique_lock<std::mutex> lock(deadlinesLock_);

		while (!submitThreadNotified_)
			newBlockIncoming_.wait(lock);

		//MinerLogger::write("submitter-thread: outer loop", TextType::System);
		lock.unlock();

		while (running_ && submitThreadNotified_)
		{
			lock.lock();
			newBlockIncoming_.wait_for(lock, std::chrono::seconds(1));

			//MinerLogger::write("submitter-thread: inner loop", TextType::System);

			for (auto& accountDeadlines : deadlines_)
			{
				auto deadline = accountDeadlines.second.getBest();

				if (deadline == nullptr)
					continue;

				if (!deadline->isOnTheWay())
				{
					auto createSendThread = true;

					if (getTargetDeadline() > 0 && deadline->getDeadline() >= getTargetDeadline())
					{
						createSendThread = false;

						MinerLogger::write("Nonce is higher then the target deadline of the pool (" +
							deadlineFormat(getTargetDeadline()) + ")", TextType::Debug);
					}

					deadline->send();

					if (createSendThread)
					{
						NonceSubmitter{*this, deadline}.startSubmit();
					}
				}
			}

			submitThreadNotified_ = !std::all_of(plotReaders_.begin(), plotReaders_.end(), [](const std::shared_ptr<PlotListReader>& reader)
												{
													return reader->isDone();
												});

			lock.unlock();
		}

		//sockets_.fill();

		//MinerLogger::write("submitter-thread: finished block", TextType::System);
	}
}

void Burst::Miner::submitNonce(uint64_t nonce, uint64_t accountId, uint64_t deadline, std::string plotFile)
{
	std::lock_guard<std::mutex> mutex(deadlinesLock_);

	auto bestDeadline = deadlines_[accountId].getBest();

	// is the new nonce better then the best one we already have?
	if (bestDeadline == nullptr || bestDeadline->getDeadline() > deadline)
	{
		deadlines_[accountId].add({ nonce, deadline, accountId, blockHeight_, plotFile });

		if (MinerConfig::getConfig().output.nonceFound)
		{
			auto message = NxtAddress(accountId).to_string() + ": nonce found (" + Burst::deadlineFormat(deadline) + ")";
			
			if (MinerConfig::getConfig().output.nonceFoundPlot)
				message += " in " + plotFile;
			
			MinerLogger::write(message, TextType::Unimportant);
		}
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

	Request request(MinerConfig::getConfig().createSession(HostType::MiningInfo));

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

			// on HTTP/1.1 connection get not close so we need to use it further
			//if (MinerConfig::getConfig().getHttp() == 1)
			//transferSocket(response, miningInfoSocket_);

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

/*std::unique_ptr<Burst::Socket> Burst::Miner::getSocket()
{
	return MinerConfig::getConfig().createSocket();
	//return sockets_.getSocket();
}

std::unique_ptr<Poco::Net::HTTPClientSession> Burst::Miner::createSession()
{
	static auto ip = MinerConfig::getConfig().urlPool.getIp();
	static auto port = static_cast<Poco::UInt16>(MinerConfig::getConfig().urlPool.getPort());
	static auto timeout = secondsToTimespan(MinerConfig::getConfig().getTimeout());

	auto session = std::make_unique<Poco::Net::HTTPClientSession>(ip, port);
	session->setTimeout(timeout);

	return session;
}

std::unique_ptr<Poco::Net::HTTPClientSession> Burst::Miner::createWalletSession()
{
	static auto ip = MinerConfig::getConfig().getWalletUrl().getIp();
	static auto port = static_cast<Poco::UInt16>(MinerConfig::getConfig().getWalletUrl().getPort());
	static auto timeout = secondsToTimespan(MinerConfig::getConfig().getTimeout());

	std::unique_ptr<Poco::Net::HTTPClientSession> session = std::make_unique<Poco::Net::HTTPSClientSession>(ip, port);
	session->setTimeout(timeout);

	return session;
}*/
