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
#include <Poco/Logger.h>
#include <Poco/SplitterChannel.h>
#include "Output.hpp"
#include <Poco/UTF8String.h>

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

template <typename T>
T getOrAdd(Poco::JSON::Object::Ptr object, const std::string& key, T defaultValue)
{
	auto json = object->get(key);

	if (json.isEmpty())
		object->set(key, defaultValue);
	else if (json.type() == typeid(T))
		return json.extract<T>();

	return defaultValue;
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
		log_critical(MinerLogger::config, "Unable to open config %s", configPath);
		return false;
	}

	plotList_.clear();

	Poco::JSON::Parser parser;
	Poco::JSON::Object::Ptr config;
	
	std::string jsonStr((std::istreambuf_iterator<char>(inputFileStream)),
		std::istreambuf_iterator<char>());
	
	inputFileStream.close();

	try
	{
		config = parser.parse(jsonStr).extract<Poco::JSON::Object::Ptr>();
	}
	catch (Poco::JSON::JSONException& exc)
	{		
		log_error(MinerLogger::config,
			"There is an error in the config file!\n"
			"%s",
			exc.displayText()
		);

		log_current_stackframe(MinerLogger::config);

		// dont forget to close the file
		if (inputFileStream.is_open())
			inputFileStream.close();

		return false;
	}

	// logging
	{
		Poco::JSON::Object::Ptr loggingObj = nullptr;
		
		if (config->has("logging"))
			loggingObj = config->get("logging").extract<Poco::JSON::Object::Ptr>();

		if (!loggingObj.isNull())
		{
			try
			{
				auto logPathObj = loggingObj->get("path");
				std::string logPath = "";

				if (logPathObj.isEmpty())
				{
					loggingObj->set("path", "");
				}
				else
				{
					logPath = logPathObj.extract<std::string>();

					if (!logPath.empty())
						log_system(MinerLogger::config, "Changing path for log file to\n\t%s", logPath);

					logPath = MinerLogger::setFilePath(logPath);
				}

				pathLogfile_ = logPath;
			}
			catch (Poco::Exception& exc)
			{
				log_fatal(MinerLogger::config, "Could not set path for log-file!\n%s", exc.displayText());
			}

			// setup logger
			for (auto& name : MinerLogger::channelNames)
			{
				if (loggingObj->has(name))
					MinerLogger::setChannelPriority(name, loggingObj->get(name).extract<std::string>());
				else
					loggingObj->set(name, MinerLogger::getChannelPriority(name));
			}
		}
		// if it dont exist, we create it!
		else
		{
			loggingObj = new Poco::JSON::Object;
			loggingObj->set("path", "");

			for (auto& name : MinerLogger::channelNames)
				loggingObj->set(name, MinerLogger::getChannelPriority(name));

			config->set("logging", loggingObj);
		}
	}

	// output
	{
		auto outputObj = config->getObject("output");

		// output
		if (outputObj.isNull())
		{
			outputObj = new Poco::JSON::Object;
			config->set("output", outputObj);
		}

		auto checkCreateFunc = [&outputObj](int id, const std::string& name)
		{
			auto obj = outputObj->get(name);

			if (!obj.isEmpty() && obj.isBoolean())
				MinerLogger::setOutput(id, obj.extract<bool>());
			else
				outputObj->set(name, MinerLogger::hasOutput(id));
		};
		
		checkCreateFunc(LastWinner, "lastWinner");
		checkCreateFunc(NonceFound, "nonceFound");
		checkCreateFunc(NonceOnTheWay, "nonceOnTheWay");
		checkCreateFunc(NonceSent, "nonceSent");
		checkCreateFunc(NonceConfirmed, "nonceConfirmed");
		checkCreateFunc(PlotDone, "plotDone");
		checkCreateFunc(DirDone, "dirDone");
	}

	// urls
	{
		auto checkCreateFunc = [&config](const std::string& name, Url& url,
			const std::string& defaultScheme, unsigned short defaultPort, const std::string& createUrl)
		{
			auto var = getOrAdd(config, name, createUrl);
			url = {var, defaultScheme, defaultPort};
			config->set(name, url.getUri().toString());
		};
		
		checkCreateFunc("poolUrl", urlPool_, "http", 8124, "http://pool.burst-team.us:8124");
		checkCreateFunc("miningInfoUrl", urlMiningInfo_, "http", 8124, "");
		checkCreateFunc("walletUrl", urlWallet_, "https", 8128, "https://wallet.burst-team.us:8128");
		checkCreateFunc("serverUrl", serverUrl_, "http", 8080, "http://localhost:8080");
 
		if (urlMiningInfo_.empty() && !urlPool_.empty())
		{
			urlMiningInfo_ = urlPool_;
			config->set("miningInfoUrl", urlPool_.getUri().toString());
		}
	}

	// plots
	{
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
			else if (plotsDyn.isEmpty())
			{
				Poco::JSON::Array::Ptr arr = new Poco::JSON::Array;
				config->set("plots", arr);
			}
			else
			{
				log_warning(MinerLogger::config, "Invalid plot file or directory in config file %s\n%s",
					configPath, plotsDyn.toString());
			}
		}
		catch (Poco::Exception& exc)
		{		
			log_error(MinerLogger::config,
				"Error while reading plot files!\n"
				"%s",
				exc.displayText()
			);

			log_current_stackframe(MinerLogger::config);
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
					log_debug(MinerLogger::config, "Plotfile with invalid path!\n%s", plotFile->getPath());
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
	}

	submission_max_retry_ = getOrAdd(config, "submissionMaxRetry", 3);
	maxBufferSizeMB = getOrAdd(config, "maxBufferSizeMB", 128);

	if (maxBufferSizeMB == 0)
		maxBufferSizeMB = 128;

	http_ = config->optValue("http", 1u);
	confirmedDeadlinesPath_ = config->optValue<std::string>("confirmed deadlines", "");
	timeout_ = getOrAdd(config, "timeout", 30.f);
	startServer_ = getOrAdd(config, "Start Server", false);
	miningIntensity_ = getOrAdd(config, "miningIntensity", 2);
	maxPlotReaders_ = getOrAdd(config, "maxPlotReaders", 0u);

	// target deadline
	{
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

				log_error(MinerLogger::config, "The target deadline is not a valid!\n"
					"Expected a number (amount of seconds) or a formated string (1m 1d 11:11:11)\n"
					"Got: %s", targetDeadline.toString());
			}
		}
		else
		{
			config->set("targetDeadline", "0y 0m 0d 00:00:00");
			targetDeadline_ = 0;
		}
	}

	// passphrase
	{
		try
		{
			auto passphraseJson = config->get("passphrase");
			Poco::JSON::Object::Ptr passphrase = nullptr;

			if (!passphraseJson.isEmpty())
				passphrase = passphraseJson.extract<Poco::JSON::Object::Ptr>();
			
			if (passphrase.isNull())
			{
				passphrase = new Poco::JSON::Object;
				config->set("passphrase", passphrase);
			}

			log_debug(MinerLogger::config, "Reading passphrase...");

			auto decrypted = Poco::UTF8::unescape(getOrAdd<std::string>(passphrase, "decrypted", ""));
			auto encrypted = Poco::UTF8::unescape(getOrAdd<std::string>(passphrase, "encrypted", ""));
			auto salt = Poco::UTF8::unescape(getOrAdd<std::string>(passphrase, "salt", ""));
			auto key = Poco::UTF8::unescape(getOrAdd<std::string>(passphrase, "key", ""));
			auto iterations = getOrAdd(passphrase, "iterations", 0u);
			auto deleteKey = getOrAdd(passphrase, "deleteKey", false);
			auto algorithm = getOrAdd<std::string>(passphrase, "algorithm", "aes-256-cbc");

			if (!encrypted.empty() && !key.empty() && !salt.empty())
			{
				log_debug(MinerLogger::config, "Encrypted passphrase found, trying to decrypt...");

				passPhrase_ = decrypt(encrypted, algorithm, key, salt, iterations);

				if (!passPhrase_.empty())
					log_debug(MinerLogger::config, "Passphrase decrypted!");

				if (deleteKey)
				{
					log_debug(MinerLogger::config, "Passhrase.deleteKey == true, deleting the key...");
					passphrase->set("key", "");
				}
			}

			// there is a decrypted passphrase, we need to encrypt it
			if (!decrypted.empty())
			{
				log_debug(MinerLogger::config, "Decrypted passphrase found, trying to encrypt...");

				encrypted = encrypt(decrypted, algorithm, key, salt, iterations);
				passPhrase_ = decrypted;

				if (!encrypted.empty())
				{
					passphrase->set("decrypted", "");
					passphrase->set("encrypted", Poco::UTF8::escape(encrypted));
					passphrase->set("salt", Poco::UTF8::escape(salt));
					passphrase->set("key", Poco::UTF8::escape(key));
					passphrase->set("iterations", iterations);

					log_debug(MinerLogger::config, "Passphrase encrypted!\n"
						"encrypted: %s\n"
						"salt: %s\n"
						"key: %s\n"
						"iterations: %u",
						encrypted, salt, std::string(key.size(), '*'), iterations
					);
				}
			}
		}
		catch (Poco::Exception& exc)
		{
			log_error(MinerLogger::config,
				"Error while reading passphrase in config file!\n"
				"%s",
				exc.displayText()
			);

			log_current_stackframe(MinerLogger::config);
		}
	}
	
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
		log_warning(MinerLogger::config, "%s is an invalid file/dir (syntax), skipping it!", fileOrPath);
		return false;
	}
		
	Poco::File fileOrDir{ path };
	
	if (!fileOrDir.exists())
	{
		log_warning(MinerLogger::config, "Plot file/dir does not exist: '%s'", path.toString());
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

uint32_t Burst::MinerConfig::getMaxPlotReaders() const
{
	return maxPlotReaders_;
}

const Poco::Path& Burst::MinerConfig::getPathLogfile() const
{
	return pathLogfile_;
}
