//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#include "MinerProtocol.h"
#include "MinerLogger.h"
#include "MinerConfig.h"
#include "nxt/nxt_address.h"
#include "rapidjson/document.h"
#include "Miner.h"
#include "MinerUtil.h"
#include "Request.hpp"
#include "Response.hpp"
#include "Socket.hpp"

Burst::MinerProtocol::MinerProtocol()
	: miner(nullptr)
{
	this->running = false;
	this->currentBaseTarget = 0;
	this->currentBlockHeight = 0;
}

Burst::MinerProtocol::~MinerProtocol()
{
	this->stop();
}

void Burst::MinerProtocol::stop()
{
	this->running = false;
}

uint64_t Burst::MinerProtocol::getTargetDeadline() const
{
	return targetDeadline;
}

bool Burst::MinerProtocol::run(Miner* miner)
{
	auto& config = MinerConfig::getConfig();
	this->miner = miner;
	auto errors = 0u;

	if (config.urlPool.getIp().empty() ||
		config.urlMiningInfo.getIp().empty())
		return false;

	//MinerLogger::write("Submission Max Delay : " + std::to_string(config.submissionMaxDelay), TextType::System);
	MinerLogger::write("Submission Max Retry : " + std::to_string(config.getSubmissionMaxRetry()), TextType::System);
	MinerLogger::write("Receive Max Retry : " + std::to_string(config.getReceiveMaxRetry()), TextType::System);
	MinerLogger::write("Send Max Retry : " + std::to_string(config.getSendMaxRetry()), TextType::System);
	MinerLogger::write("Send-Timeout : " + std::to_string(config.getSendTimeout()) + " seconds", TextType::System);
	MinerLogger::write("Receive-Timeout : " + std::to_string(config.getReceiveTimeout()) + " seconds", TextType::System);
	MinerLogger::write("Buffer Size : " + std::to_string(config.maxBufferSizeMB) + " MB", TextType::System);
	MinerLogger::write("Http : HTTP/1." + std::to_string(config.getHttp()), TextType::System);
	MinerLogger::write("Pool Host : " + config.urlPool.getCanonical() + ":" + std::to_string(config.urlPool.getPort()) +
		" (" + config.urlPool.getIp() + ")", TextType::System);
	MinerLogger::write("Mininginfo URL : " + config.urlMiningInfo.getCanonical() + ":" + std::to_string(config.urlMiningInfo.getPort()) +
		" (" + config.urlMiningInfo.getIp() + ")", TextType::System);

	running = true;

	while (running)
	{
		if (!getMiningInfo())
			++errors;

		// we have a tollerance of 3 times of not being able to fetch mining infos, before its a real error
		if (errors >= 3)
		{
			// reset error-counter and show error-message in console
			MinerLogger::write("Could not get block infos!", TextType::Error);
			errors = 0;
		}

		std::this_thread::sleep_for(std::chrono::seconds(3));
	}

	return false;
}

bool Burst::MinerProtocol::getMiningInfo()
{
	Request request(MinerConfig::getConfig().createSocket());

	auto response = request.sendGet("/burst?requestType=getMiningInfo");

	std::string responseData;

	if (response.receive(responseData))
	{
		HttpResponse httpResponse(responseData);
		rapidjson::Document body;

		body.Parse<0>(httpResponse.getMessage().c_str());

		if (body.GetParseError() == nullptr)
		{
			if (body.HasMember("baseTarget"))
			{
				std::string baseTargetStr = body["baseTarget"].GetString();
				this->currentBaseTarget = std::stoull(baseTargetStr);
			}

			if (body.HasMember("generationSignature"))
			{
				this->gensig = body["generationSignature"].GetString();
			}

			if (body.HasMember("targetDeadline"))
			{
				targetDeadline = body["targetDeadline"].GetUint64();
			}

			if (body.HasMember("height"))
			{
				std::string newBlockHeightStr = body["height"].GetString();
				auto newBlockHeight = std::stoull(newBlockHeightStr);

				if (newBlockHeight > this->currentBlockHeight)
					this->miner->updateGensig(this->gensig, newBlockHeight, this->currentBaseTarget);

				this->currentBlockHeight = newBlockHeight;
			}

			return true;
		}


		MinerLogger::write("Error on getting new block-info!", TextType::Error);
		MinerLogger::write(body.GetParseError(), TextType::Error);
		MinerLogger::write("Full response:", TextType::Error);
		MinerLogger::write(httpResponse.getResponse(), TextType::Error);

		return false;
	}

	return false;
}
