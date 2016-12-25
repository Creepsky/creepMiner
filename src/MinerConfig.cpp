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
#ifdef _WIN32
#	include <win/dirent.h>
#endif
#include <sys/stat.h>
#include "SocketDefinitions.hpp"
#include "Socket.hpp"
#include <memory>

void Burst::MinerConfig::rescan()
{
	this->readConfigFile(this->configPath_);
}

Burst::PlotFile::PlotFile(std::string&& path, size_t size)
	: path(std::move(path)), size(size)
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
	configPath_ = configPath;
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

	if (configDoc.HasMember("sendMaxRetry"))
	{
		if (configDoc["sendMaxRetry"].IsNumber())
			send_max_retry_ = configDoc["sendMaxRetry"].GetInt();
		else
			send_max_retry_ = 3;
	}

	if (configDoc.HasMember("receiveMaxRetry"))
	{
		if (configDoc["receiveMaxRetry"].IsNumber())
			receive_max_retry_ = configDoc["receiveMaxRetry"].GetInt();
		else
			receive_max_retry_ = 3;
	}

	if (configDoc.HasMember("sendTimeout"))
	{
		if (configDoc["sendTimeout"].IsNumber())
			send_timeout_ = static_cast<float>(configDoc["sendTimeout"].GetDouble());
		else
			send_timeout_ = 5.f;
	}

	if (configDoc.HasMember("receiveTimeout"))
	{
		if (configDoc["receiveTimeout"].IsNumber())
			receive_timeout_ = static_cast<float>(configDoc["receiveTimeout"].GetDouble());
		else
			receive_timeout_ = 5.f;
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

	if (configDoc.HasMember("max submit threads"))
		if (configDoc["max submit threads"].IsUint())
			maxSubmitThreads_ = configDoc["max submit threads"].GetUint();

	if (configDoc.HasMember("timeout"))
		if (configDoc["timeout"].IsNumber())
			timeout_ = static_cast<float>(configDoc["timeout"].GetDouble());

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
	return receive_timeout_;
}

float Burst::MinerConfig::getSendTimeout() const
{
	return send_timeout_;
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

size_t Burst::MinerConfig::getMaxSubmitThreads() const
{
	return maxSubmitThreads_;
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
	return createSocket(hostType)->createSession();
}

std::unique_ptr<Poco::Net::HTTPClientSession> Burst::MinerConfig::createSession(const std::string& url) const
{
	// return appropriate session for the type of url
	// for example https, http(, ftp maybe)
	return nullptr;
}

Burst::MinerConfig& Burst::MinerConfig::getConfig()
{
	static MinerConfig config;
	return config;
}

bool Burst::MinerConfig::addPlotLocation(const std::string& fileOrPath)
{
	struct stat info;
	auto statResult = stat(fileOrPath.c_str(), &info);

	if (statResult != 0)
	{
		MinerLogger::write(fileOrPath + " does not exist or can not be read", TextType::Error);
		return false;
	}

	// its a directory
	if (info.st_mode & S_IFDIR)
	{
		auto dirPath = fileOrPath;
		auto sizeBefore = plotList_.size();

		if (dirPath[dirPath.length() - 1] != PATH_SEPARATOR)
			dirPath += PATH_SEPARATOR;

		DIR* dir;
		struct dirent* ent;
		size_t size = 0;

		if ((dir = opendir(dirPath.c_str())) != nullptr)
		{
			while ((ent = readdir(dir)) != nullptr)
			{
				auto plotFile = addPlotFile(dirPath + std::string(ent->d_name));

				if (plotFile != nullptr)
					size += plotFile->getSize();
			}

			closedir(dir);
		}
		else
		{
			MinerLogger::write("failed reading file or directory " + fileOrPath, TextType::Error);
			closedir(dir);
			return false;
		}

		MinerLogger::write(std::to_string(this->plotList_.size() - sizeBefore) + " plot(s) found at " + fileOrPath
						   + ", total size: " + gbToString(size) + " GB", TextType::System);
	}
	// its a single plot file
	else
	{
		auto plotFile = this->addPlotFile(fileOrPath);

		if (plotFile != nullptr)
			MinerLogger::write("plot found at " + fileOrPath + ", total size: " + gbToString(plotFile->getSize()) + " GB", TextType::System);
	}

	return true;
}

std::shared_ptr<Burst::PlotFile> Burst::MinerConfig::addPlotFile(const std::string& path)
{
	if (isValidPlotFile(path))
	{
		for (size_t i = 0; i < this->plotList_.size(); i++)
			if (this->plotList_[i]->getPath() == path)
				return this->plotList_[i];

		std::ifstream in(path, std::ifstream::ate | std::ifstream::binary);

		auto plotFile = std::make_shared<PlotFile>(std::string(path), in.tellg());

		plotList_.emplace_back(plotFile);

		in.close();

		// MinerLogger::write("Plot " + std::to_string(this->plotList.size()) + ": " + file);

		return plotFile;
	}

	return nullptr;
}
