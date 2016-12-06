//
//  MinerConfig.cpp
//  cryptoport-miner
//
//  Created by Uray Meiviar on 9/15/14.
//  Copyright (c) 2014 Miner. All rights reserved.
//

#include "MinerConfig.h"
#include "MinerLogger.h"
#include "MinerUtil.h"
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
	this->readConfigFile(this->configPath);
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
	this->configPath = configPath;
	std::ifstream inputFileStream;

	plotList.clear();

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
				}
			}
		}
	}

	if (configDoc.HasMember("poolUrl"))
	{
		urlPool = { configDoc["poolUrl"].GetString() };
	}
	else
	{
		MinerLogger::write("No poolUrl is defined in config file " + configPath, TextType::Error);
		return false;
	}

	if (configDoc.HasMember("miningInfoUrl"))
	{
		urlMiningInfo = { configDoc["miningInfoUrl"].GetString() };
	}
	// if no getMiningInfoUrl and port are defined, we assume that the pool is the source
	else
	{
		urlMiningInfo = urlPool;
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

		for (auto plotFile : plotList)
			totalSize += plotFile->getSize();

		MinerLogger::write("Total plots size: " + gbToString(totalSize) + " GB", TextType::System);
	}
	else
	{
		MinerLogger::write("No plot file or directory defined in config file " + configPath, TextType::Error);
		return false;
	}

	if (this->plotList.empty())
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

	return true;
}

const std::string& Burst::MinerConfig::getPath() const
{
	return configPath;
}

const std::vector<std::shared_ptr<Burst::PlotFile>>& Burst::MinerConfig::getPlotFiles() const
{
	return plotList;
}

uintmax_t Burst::MinerConfig::getTotalPlotsize() const
{
	uintmax_t sum = 0;

	for (auto plotFile : plotList)
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

std::unique_ptr<Burst::Socket> Burst::MinerConfig::createSocket() const
{
	auto socket = std::make_unique<Socket>(getSendTimeout(), getReceiveTimeout());
	socket->connect(urlPool.getIp(), urlPool.getPort());
	return socket;
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
		auto sizeBefore = plotList.size();

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

		MinerLogger::write(std::to_string(this->plotList.size() - sizeBefore) + " plot(s) found at " + fileOrPath
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
		for (size_t i = 0; i < this->plotList.size(); i++)
			if (this->plotList[i]->getPath() == path)
				return this->plotList[i];

		std::ifstream in(path, std::ifstream::ate | std::ifstream::binary);

		auto plotFile = std::make_shared<PlotFile>(std::string(path), in.tellg());

		this->plotList.emplace_back(plotFile);

		in.close();

		// MinerLogger::write("Plot " + std::to_string(this->plotList.size()) + ": " + file);

		return plotFile;
	}

	return nullptr;
}
