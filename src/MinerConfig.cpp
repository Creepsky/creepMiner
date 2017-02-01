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
#include "SocketDefinitions.hpp"
#include "Socket.hpp"
#include <memory>
#include <Poco/File.h>
#include <Poco/Path.h>
#include <Poco/DirectoryIterator.h>
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Array.h>
#include <Poco/NestedDiagnosticContext.h>
#include <unordered_map>
#include <Poco/SHA1Engine.h>
#include <Poco/DigestStream.h>
#include "PlotSizes.hpp"

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
	poco_ndc(readConfigFile);
	std::ifstream inputFileStream;

	// first we open the config file
	try
	{
		inputFileStream.open(configPath);
	}
	catch (...)
	{
		MinerLogger::write("unable to open config " + configPath, TextType::Error);
		return false;
	}

	plotList_.clear();

	Poco::JSON::Parser parser;
	Poco::JSON::Object::Ptr config;
	
	try
	{
		config = parser.parse(inputFileStream).extract<Poco::JSON::Object::Ptr>();
	}
	catch (Poco::JSON::JSONException& exc)
	{
		std::vector<std::string> lines = {
			"there is an error in the config file!",
			exc.displayText()
		};
				
		MinerLogger::writeStackframe(lines);

		// dont forget to close the file
		if (inputFileStream.is_open())
			inputFileStream.close();

		return false;
	}
	
	auto outputObj = config->getObject("output");

	if (!outputObj.isNull())
	{
		output.progress = outputObj->optValue("progress", true);
		output.debug = outputObj->optValue("debug", false);
		output.nonceFound = outputObj->optValue("nonce found", true);
		output.nonceFoundPlot = outputObj->optValue("nonce found plot", false);
		output.nonceConfirmedPlot = outputObj->optValue("nonce confirmed plot", false);
		output.plotDone = outputObj->optValue("plot done", false);
		output.dirDone = outputObj->optValue("dir done", false);
		output.lastWinner = outputObj->optValue("last winner", true);

		auto errorObj = outputObj->getObject("error");

		if (!errorObj.isNull())
		{
			output.error.request = errorObj->optValue("request", false);
			output.error.response = errorObj->optValue("response", false);
		}
	}

	auto urlPoolStr = config->optValue<std::string>("poolUrl", "");
	
	urlPool_ = urlPoolStr;
	// if no getMiningInfoUrl and port are defined, we assume that the pool is the source
	urlMiningInfo_ = config->optValue("miningInfoUrl", urlPoolStr);
	urlWallet_ = config->optValue<std::string>("walletUrl", "");
		
	try
	{
		auto plotsDyn = config->get("plots");
						
		if (plotsDyn.type() == typeid(Poco::JSON::Array::Ptr))
		{
			auto plots = plotsDyn.extract<Poco::JSON::Array::Ptr>();
			
			for (auto& plot : *plots)
				addPlotLocation(plot.convert<std::string>());
		}
		else if (plotsDyn.isString())
		{
			addPlotLocation(plotsDyn.extract<std::string>());
		}
		else
		{
			MinerLogger::write("Invalid plot file or directory in config file " + configPath, TextType::Error);
			MinerLogger::write(plotsDyn.toString(), TextType::Error);
		}
	}
	catch (Poco::Exception& exc)
	{
		std::vector<std::string> lines = {
			"error while reading plot files!",
			exc.what()
		};
		
		MinerLogger::writeStackframe(lines);
	}

	// combining all plotfiles to lists of plotfiles on the same device
	{
		Poco::SHA1Engine sha;
		Poco::DigestOutputStream shaStream{sha};

		for (const auto plotFile : getPlotFiles())
		{
			Poco::Path path{ plotFile->getPath() };

			shaStream << plotFile->getPath();

			auto dir = path.getDevice();

			// if its empty its unix and we have to look for the top dir
			if (dir.empty() && path.depth() > 0)
				dir = path.directory(0);

			// if its now empty, we have a really weird plotfile and skip it
			if (dir.empty())
			{
				std::vector<std::string> lines = {
					"Plotfile with invalid path!",
					plotFile->getPath()
				};

				MinerLogger::write(lines, TextType::Debug);
				continue;
			}

			auto iter = plotDirs_.find(dir);

			if (iter == plotDirs_.end())
				plotDirs_.emplace(std::make_pair(dir, PlotList {}));

			plotDirs_[dir].emplace_back(plotFile);
		}

		shaStream << std::flush;
		plotsHash_ = Poco::SHA1Engine::digestToHex(sha.digest());

		// we remember our total plot size
		PlotSizes::set(plotsHash_, getTotalPlotsize() / 1024 / 1024 / 1024);
	}

	submission_max_retry_ = config->optValue("submissionMaxRetry", 3u);
	maxBufferSizeMB = config->optValue("maxBufferSizeMB", 64u);

	if (maxBufferSizeMB == 0)
		maxBufferSizeMB = 1;

	http_ = config->optValue("http", 1u);
	confirmedDeadlinesPath_ = config->optValue<std::string>("confirmed deadlines", "");
	timeout_ = config->optValue("timeout", 30.f);
	startServer_ = config->optValue("Start Server", false);
	serverUrl_ = config->optValue<std::string>("serverUrl", "");
	miningIntensity_ = std::max(config->optValue("miningIntensity", 1), 1);

	auto targetDeadline = config->get("targetDeadline");
	
	if (!targetDeadline.isEmpty())
	{
		// could be the raw deadline
		if (targetDeadline.isInteger())
			targetDeadline_ = targetDeadline.convert<uint64_t>();
		// or a formated string
		else if (targetDeadline.isString())
			targetDeadline_ = formatDeadline(targetDeadline.convert<std::string>());
		else
		{
			targetDeadline_ = 0;
			
			std::vector<std::string> lines = {
				"the target deadline is not a valid!",
				"expected a number (amount of seconds) or a formated string (1m 1d 11:11:11)",
				"got: " + targetDeadline.toString()
			};
			
			MinerLogger::write(lines, TextType::Error);
		}
	}

	try
	{
		auto passphraseJson = config->get("passphrase");
		Poco::JSON::Object::Ptr passphrase = nullptr;

		if (!passphraseJson.isEmpty())
			passphraseJson.extract<Poco::JSON::Object::Ptr>();

		if (!passphrase.isNull())
		{
			auto decrypted = passphrase->optValue<std::string>("decrypted", "");
			auto encrypted = passphrase->optValue<std::string>("encrypted", "");
			auto salt = passphrase->optValue<std::string>("salt", "");
			auto key = passphrase->optValue<std::string>("key", "");
			auto iterations = passphrase->optValue<uint32_t>("iterations", 0);
			auto deleteKey = passphrase->optValue<uint32_t>("deleteKey", false);
			auto algorithm = passphrase->optValue<std::string>("algorithm", "aes-256-cbc");

			if (!encrypted.empty() && !key.empty() && !salt.empty())
			{
				passPhrase_ = decrypt(encrypted, algorithm, key, salt, iterations);

				if (deleteKey)
					passphrase->set("key", "");
			}

			// there is a decrypted passphrase, we need to encrypt it
			if (!decrypted.empty())
			{
				encrypted = encrypt(decrypted, algorithm, key, salt, iterations);
				passPhrase_ = decrypted;

				passphrase->set("decrypted", "");
				passphrase->set("encrypted", encrypted);
				passphrase->set("salt", salt);
				passphrase->set("key", key);
				passphrase->set("iterations", iterations);
			}
		}
	}
	catch (Poco::Exception& exc)
	{
		std::vector<std::string> lines = {
			"Error while reading passphrase in config file!",
			exc.displayText()
		};

		MinerLogger::writeStackframe(lines);
	}
	
	inputFileStream.close();

	std::ofstream outputFileStream{configPath};

	if (outputFileStream.is_open())
	{
		config->stringify(outputFileStream, 4);
		outputFileStream.close();
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
	{
		auto session = url->createSession();
		session->setTimeout(secondsToTimespan(getTimeout()));
		return session;
	}

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
		MinerLogger::write(fileOrPath + " is an invalid file/dir (syntax), skipping it!", TextType::Error);
		return false;
	}
		
	Poco::File fileOrDir{ path };
	
	if (!fileOrDir.exists())
	{
		MinerLogger::write("plot file/dir does not exist: '" + path.toString() + "'", TextType::Error);
		return false;
	}
	
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

uint32_t Burst::MinerConfig::getMiningIntensity() const
{
	return miningIntensity_;
}

const std::unordered_map<std::string, Burst::MinerConfig::PlotList>& Burst::MinerConfig::getPlotList() const
{
	return plotDirs_;
}

const std::string& Burst::MinerConfig::getPlotsHash() const
{
	return plotsHash_;
}

const std::string& Burst::MinerConfig::getPassphrase() const
{
	return passPhrase_;
}
