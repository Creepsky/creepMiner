//
//  MinerConfig.cpp
//  cryptoport-miner
//
//  Created by Uray Meiviar on 9/15/14.
//  Copyright (c) 2014 Miner. All rights reserved.
//

#include "MinerConfig.hpp"
#include "MinerLogger.hpp"
#include "MinerUtil.hpp"
#include <fstream>
#include "rapidjson/document.h"
#include <sstream>
#include <sys/stat.h>
#include "SocketDefinitions.hpp"
#include "Socket.hpp"
#include <memory>
#include <Poco/File.h>
#include <Poco/Path.h>
#include <Poco/DirectoryIterator.h>

void Burst::MinerConfig::rescan()
{
	readConfigFile(configPath_);
}

Burst::PlotFile::PlotFile(std::string&& path, size_t size)
	: path(move(path)), size(size)
{}

const std::string& Burst::PlotFile::getPath() const
{
	return path;
}

size_t Burst::PlotFile::getSize() const
{
	return size;
}

bool Burst::MinerConfig::readConfigFile(const std::string& configPath)
{
	std::ifstream inputFileStream;

	plotList_.clear();

	auto exceptionMask = inputFileStream.exceptions() | std::ios::failbit;
	inputFileStream.exceptions(exceptionMask);

	try
	{
		inputFileStream.open(configPath);
	}
	catch (...)
	{
		MinerLogger::write("unable to open file " + configPath, TextType::Error);
		return false;
	}

	std::stringstream configContent;
	configContent << inputFileStream.rdbuf();
	inputFileStream.close();
	std::string configContentStr = configContent.str();

	rapidjson::Document configDoc;
	configDoc.Parse<0>(configContentStr.c_str());

	if (configDoc.GetParseError() != nullptr)
	{
		std::string parseError = configDoc.GetParseError();
		size_t parseErrorLoc = configDoc.GetErrorOffset();
		MinerLogger::write("Parsing Error : " + parseError, TextType::Error);
		size_t sampleLen = 16;
		if (configContentStr.length() - parseErrorLoc < 16)
		{
			sampleLen = configContentStr.length() - parseErrorLoc;
		}
		MinerLogger::write("--> " + configContentStr.substr(parseErrorLoc, sampleLen) + "...", TextType::Error);
		return false;
	}
	
	if (configDoc.HasMember("output"))
	{
		if (configDoc["output"].IsArray())
		{
			auto& outputs = configDoc["output"];

			for (auto i = 0u; i < outputs.Size(); ++i)
			{
				auto& setting = outputs[i];

				if (!setting.IsString())
					continue;

				std::string value(setting.GetString());

				auto set = true;
				auto start = 0u;

				// if the first char is a '-', the output needs to be unset
				if (!value.empty())
				{
					auto firstCharacter = *value.begin();

					if (firstCharacter == '-')
					{
						set = false;
						start = 1;
					}
					else if (firstCharacter == '+')
					{
						set = true;
						start = 1;
					}
					else
					{
						set = true;
					}
				}

				if (value.size() > start)
				{
					auto realValue = value.substr(start);

					if (realValue == "progress")
						this->output.progress = set;
					else if (realValue == "debug")
						this->output.debug = set;
					else if (realValue == "nonce found")
						output.nonceFound = set;
					else if (realValue == "nonce found plot")
						output.nonceFoundPlot = set;
					else if (realValue == "nonce confirmed plot")
						output.nonceConfirmedPlot = set;
					else if (realValue == "plot done")
						output.plotDone = set;
					else if (realValue == "dir done")
						output.dirDone = set;
					else if (realValue == "last winner")
						output.lastWinner = set;
				}
			}
		}
	}

	if (configDoc.HasMember("poolUrl"))
	{
		urlPool_ = { configDoc["poolUrl"].GetString() };
	}
	else
	{
		MinerLogger::write("No poolUrl is defined in config file " + configPath, TextType::Error);
		return false;
	}

	if (configDoc.HasMember("walletUrl"))
	{
		urlWallet_ = { configDoc["walletUrl"].GetString() };
	}

	if (configDoc.HasMember("miningInfoUrl"))
	{
		urlMiningInfo_ = { configDoc["miningInfoUrl"].GetString() };
	}
	// if no getMiningInfoUrl and port are defined, we assume that the pool is the source
	else
	{
		urlMiningInfo_ = urlPool_;
	}

	if (configDoc.HasMember("plots"))
	{
		if (configDoc["plots"].IsArray())
		{
			for (auto itr = configDoc["plots"].Begin(); itr != configDoc["plots"].End(); ++itr)
			{
				this->addPlotLocation(itr->GetString());
			}
		}
		else if (configDoc["plots"].IsString())
		{
			this->addPlotLocation(configDoc["plots"].GetString());
		}
		else
		{
			MinerLogger::write("Invalid plot file or directory in config file " + configPath, TextType::Error);
			return false;
		}

		uint64_t totalSize = 0;

		for (auto plotFile : plotList_)
			totalSize += plotFile->getSize();

		MinerLogger::write("Total plots size: " + gbToString(totalSize) + " GB", TextType::System);
	}
	else
	{
		MinerLogger::write("No plot file or directory defined in config file " + configPath, TextType::Error);
		return false;
	}

	if (plotList_.empty())
	{
		MinerLogger::write("No valid plot file or directory in config file " + configPath, TextType::Error);
		return false;
	}

	if (configDoc.HasMember("submissionMaxRetry"))
	{
		if (configDoc["submissionMaxRetry"].IsNumber())
			submission_max_retry_ = configDoc["submissionMaxRetry"].GetInt();
		else
			submission_max_retry_ = 3;
	}

	if (configDoc.HasMember("maxBufferSizeMB"))
	{
		if (configDoc["maxBufferSizeMB"].IsNumber())
		{
			this->maxBufferSizeMB = configDoc["maxBufferSizeMB"].GetInt();
		}
		else
		{
			std::string maxBufferSizeMBStr = configDoc["maxBufferSizeMB"].GetString();
			try
			{
				this->maxBufferSizeMB = std::stoul(maxBufferSizeMBStr);
			}
			catch (...)
			{
				this->maxBufferSizeMB = 64;
			}
		}

		if (this->maxBufferSizeMB < 1)
		{
			this->maxBufferSizeMB = 1;
		}
	}

	if (configDoc.HasMember("http"))
		if (configDoc["http"].IsNumber())
			http_ = configDoc["http"].GetUint();

	if (configDoc.HasMember("confirmed deadlines"))
		if (configDoc["confirmed deadlines"].IsString())
			confirmedDeadlinesPath_ = configDoc["confirmed deadlines"].GetString();

	if (configDoc.HasMember("timeout"))
		if (configDoc["timeout"].IsNumber())
			timeout_ = static_cast<float>(configDoc["timeout"].GetDouble());

	if (configDoc.HasMember("Start Server"))
		if (configDoc["Start Server"].IsBool())
			startServer_ = configDoc["Start Server"].GetBool();

	if (configDoc.HasMember("serverUrl"))
		if (configDoc["serverUrl"].IsString())
			serverUrl_ = {configDoc["serverUrl"].GetString()};

	if (configDoc.HasMember("targetDeadline"))
	{
		if (configDoc["targetDeadline"].IsUint64())
			targetDeadline_ = configDoc["targetDeadline"].GetUint64();
		else if (configDoc["targetDeadline"].IsString())
			targetDeadline_ = formatDeadline(configDoc["targetDeadline"].GetString());
	}

	return true;
}

const std::string& Burst::MinerConfig::getPath() const
{
	return configPath_;
}

const std::vector<std::shared_ptr<Burst::PlotFile>>& Burst::MinerConfig::getPlotFiles() const
{
	return plotList_;
}

uintmax_t Burst::MinerConfig::getTotalPlotsize() const
{
	uintmax_t sum = 0;

	for (auto plotFile : plotList_)
		sum += plotFile->getSize();

	return sum;
}

float Burst::MinerConfig::getReceiveTimeout() const
{
	return getTimeout();
}

float Burst::MinerConfig::getSendTimeout() const
{
	return getTimeout();
}

float Burst::MinerConfig::getTimeout() const
{
	return timeout_;
}

const Burst::Url& Burst::MinerConfig::getPoolUrl() const
{
	return urlPool_;
}

const Burst::Url& Burst::MinerConfig::getMiningInfoUrl() const
{
	return urlMiningInfo_;
}

const Burst::Url& Burst::MinerConfig::getWalletUrl() const
{
	return urlWallet_;
}

size_t Burst::MinerConfig::getReceiveMaxRetry() const
{
	return receive_max_retry_;
}

size_t Burst::MinerConfig::getSendMaxRetry() const
{
	return send_max_retry_;
}

size_t Burst::MinerConfig::getSubmissionMaxRetry() const
{
	return submission_max_retry_;
}

size_t Burst::MinerConfig::getHttp() const
{
	return http_;
}

const std::string& Burst::MinerConfig::getConfirmedDeadlinesPath() const
{
	return confirmedDeadlinesPath_;
}

bool Burst::MinerConfig::getStartServer() const
{
	return startServer_;
}

uint64_t Burst::MinerConfig::getTargetDeadline() const
{
	return targetDeadline_;
}

const Burst::Url& Burst::MinerConfig::getServerUrl() const
{
	return serverUrl_;
}

std::unique_ptr<Burst::Socket> Burst::MinerConfig::createSocket(HostType hostType) const
{
	auto socket = std::make_unique<Socket>(getSendTimeout(), getReceiveTimeout());
	const Url* url;
	
	if (hostType == HostType::Pool)
		url = &urlPool_;
	else if (hostType == HostType::MiningInfo)
		url = &urlMiningInfo_;
	else if (hostType == HostType::Wallet)
		url = &urlWallet_;
	else
		url = nullptr;

	if (url != nullptr)
		socket->connect(url->getIp(), url->getPort());

	return socket;
}

std::unique_ptr<Poco::Net::HTTPClientSession> Burst::MinerConfig::createSession(HostType hostType) const
{
	const Url* url;

	if (hostType == HostType::Pool)
		url = &urlPool_;
	else if (hostType == HostType::MiningInfo)
		url = &urlMiningInfo_;
	else if (hostType == HostType::Wallet)
		url = &urlWallet_;
	else
		url = nullptr;

	if (url != nullptr)
		return url->createSession();

	return nullptr;
}

Burst::MinerConfig& Burst::MinerConfig::getConfig()
{
	static MinerConfig config;
	return config;
}

bool Burst::MinerConfig::addPlotLocation(const std::string& fileOrPath)
{
	Poco::Path path;

	if (!path.tryParse(fileOrPath))
	{
		MinerLogger::write(fileOrPath + " is an invalid dir (syntax), skipping it!", TextType::Error);
		return false;
	}

	Poco::File fileOrDir{ path };
	
	// its a single plot file, add it if its really a plot file
	if (fileOrDir.isFile())
	{
		return addPlotFile(fileOrPath) != nullptr;
	}

	// its a dir, so we need to parse all plot files in it and add them
	if (fileOrDir.isDirectory())
	{
		Poco::DirectoryIterator iter{ fileOrDir };
		Poco::DirectoryIterator end;

		while (iter != end)
		{
			if (iter->isFile())
				addPlotFile(*iter);
			
			++iter;
		}

		return true;
	}

	return false;
}

std::shared_ptr<Burst::PlotFile> Burst::MinerConfig::addPlotFile(const Poco::File& file)
{
	if (isValidPlotFile(file.path()))
	{
		// plot file is already in our list
		for (size_t i = 0; i < plotList_.size(); i++)
			if (plotList_[i]->getPath() == file.path())
				return plotList_[i];

		auto plotFile = std::make_shared<PlotFile>(std::string(file.path()), file.getSize());
		plotList_.emplace_back(plotFile);

		// MinerLogger::write("Plot " + std::to_string(this->plotList.size()) + ": " + file);

		return plotFile;
	}

	return nullptr;
}
