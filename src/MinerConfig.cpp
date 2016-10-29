//
//  MinerConfig.cpp
//  cryptoport-miner
//
//  Created by Uray Meiviar on 9/15/14.
//  Copyright (c) 2014 Miner. All rights reserved.
//

#include "Miner.h"

void Burst::MinerConfig::rescan()
{
	this->readConfigFile(this->configPath);
}

bool Burst::MinerConfig::readConfigFile(const std::string& configPath)
{
	this->configPath = configPath;
	std::ifstream inputFileStream;

	std::ios_base::iostate exceptionMask = inputFileStream.exceptions() | std::ios::failbit;
	inputFileStream.exceptions(exceptionMask);

	try
	{
		inputFileStream.open(configPath);
	}
	catch (...)
	{
		MinerLogger::write("unable to open file " + configPath);
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
		MinerLogger::write("Parsing Error : " + parseError);
		size_t sampleLen = 16;
		if (configContentStr.length() - parseErrorLoc < 16)
		{
			sampleLen = configContentStr.length() - parseErrorLoc;
		}
		MinerLogger::write("--> " + configContentStr.substr(parseErrorLoc, sampleLen) + "...");
		return false;
	}

	if (configDoc.HasMember("poolUrl"))
	{
		std::string poolUrl = configDoc["poolUrl"].GetString();
		auto startPos = poolUrl.find("//");

		if (startPos == std::string::npos)
			startPos = 0;

		auto hostEnd = poolUrl.find(":", startPos);

		if (hostEnd == std::string::npos)
		{
			this->poolPort = 80;
			this->poolHost = poolUrl.substr(startPos, poolUrl.length() - startPos);
		}
		else
		{
			this->poolHost = poolUrl.substr(startPos, hostEnd - startPos);
			auto poolPortStr = poolUrl.substr(hostEnd + 1, poolUrl.length() - (hostEnd + 1));

			try
			{
				this->poolPort = std::stoul(poolPortStr);
			}
			catch (...)
			{
				this->poolPort = 80;
			}
		}
	}
	else
	{
		MinerLogger::write("No poolUrl is defined in config file " + configPath);
		return false;
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
			MinerLogger::write("Invalid plot file or directory in config file " + configPath);
			return false;
		}
	}
	else
	{
		MinerLogger::write("No plot file or directory defined in config file " + configPath);
		return false;
	}

	if (this->plotList.empty())
	{
		MinerLogger::write("No valid plot file or directory in config file " + configPath);
		return false;
	}

	if (configDoc.HasMember("submissionMaxDelay"))
	{
		if (configDoc["submissionMaxDelay"].IsNumber())
		{
			this->submissionMaxDelay = configDoc["submissionMaxDelay"].GetInt();
		}
		else
		{
			std::string maxDelayStr = configDoc["submissionMaxDelay"].GetString();
			try
			{
				this->submissionMaxDelay = std::stoul(maxDelayStr);
			}
			catch (...)
			{
				this->submissionMaxDelay = 60;
			}
		}
	}

	if (configDoc.HasMember("submissionMaxRetry"))
	{
		if (configDoc["submissionMaxRetry"].IsNumber())
		{
			this->submissionMaxRetry = configDoc["submissionMaxRetry"].GetInt();
		}
		else
		{
			std::string submissionMaxRetryStr = configDoc["submissionMaxRetry"].GetString();

			try
			{
				this->submissionMaxRetry = std::stoul(submissionMaxRetryStr);
			}
			catch (...)
			{
				this->submissionMaxRetry = 3;
			}
		}
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

	return true;
}

bool Burst::MinerConfig::addPlotLocation(const std::string fileOrPath)
{
	struct stat info;
	auto statResult = stat(fileOrPath.c_str(), &info);

	if (statResult != 0)
	{
		MinerLogger::write(fileOrPath + " does not exist or can not be read");
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

		if ((dir = opendir(dirPath.c_str())) != nullptr)
		{
			while ((ent = readdir(dir)) != nullptr)
				this->addPlotFile(dirPath + std::string(ent->d_name));

			closedir(dir);
		}
		else
		{
			MinerLogger::write("failed reading file or directory " + fileOrPath);
			closedir(dir);
			return false;
		}
		
		std::cout << this->plotList.size() - sizeBefore << " plot(s) found at " << fileOrPath << std::endl;
	}
	// its a single plot file
	else
	{
		this->addPlotFile(fileOrPath);
		std::cout << "plot found at " << fileOrPath << std::endl;
	}
	return true;
}

bool Burst::MinerConfig::addPlotFile(const std::string& path)
{
	if (isValidPlotFile(path))
	{
		for (size_t i = 0; i < this->plotList.size(); i++)
			if (this->plotList[i].path == path)
				return true;

		std::ifstream in(path, std::ifstream::ate | std::ifstream::binary);

		this->plotList.emplace_back(PlotFile { path, in.tellg() });

		in.close();

		// MinerLogger::write("Plot " + std::to_string(this->plotList.size()) + ": " + file);

		return true;
	}

	return false;
}
