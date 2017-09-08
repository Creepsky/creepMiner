// ==========================================================================
// 
// creepMiner - Burstcoin cryptocurrency CPU and GPU miner
// Copyright (C)  2016-2017 Creepsky (creepsky@gmail.com)
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110 - 1301  USA
// 
// ==========================================================================

#include "MinerConfig.hpp"
#include "logging/MinerLogger.hpp"
#include "MinerUtil.hpp"
#include <fstream>
#include <memory>
#include <Poco/File.h>
#include <Poco/Path.h>
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/Array.h>
#include <Poco/NestedDiagnosticContext.h>
#include <Poco/SHA1Engine.h>
#include <Poco/DigestStream.h>
#include "plots/PlotSizes.hpp"
#include <Poco/Logger.h>
#include <Poco/SplitterChannel.h>
#include "logging/Output.hpp"
#include "plots/PlotReader.hpp"
#include "plots/Plot.hpp"

const std::string Burst::MinerConfig::WebserverUserPassphrase = "ms7zKm7QjsSOQEP13wHAWnraSp7yP7YSQdPzAjvO";
const std::string Burst::MinerConfig::WebserverPassPassphrase = "CAAwj6RTQqXZGxbNjLVqr5FwAqT7GM9Y1wppNLRp";
const std::string Burst::MinerConfig::HASH_DELIMITER = "::::";

void Burst::MinerConfig::rescan()
{
	readConfigFile(configPath_);
}

bool Burst::MinerConfig::rescanPlotfiles()
{
	log_system(MinerLogger::config, "Rescanning plot-dirs...");

	Poco::Mutex::ScopedLock lock(mutex_);

	for (auto& plotDir : plotDirs_)
		plotDir->rescan();

	const auto oldPlotsHash = plotsHash_;
	recalculatePlotsHash();
	
	if (oldPlotsHash != plotsHash_)
	{
		printConsolePlots();
		return true;
	}

	return false;
}

void Burst::MinerConfig::printConsole() const
{
	Poco::Mutex::ScopedLock lock(mutex_);

	log_system(MinerLogger::config, "Submission Max Retry : %s",
		getSubmissionMaxRetry() == 0u ? "unlimited" : std::to_string(getSubmissionMaxRetry()) + " seconds");
	
	printBufferSize();

	printUrl(HostType::Pool);
	printUrl(HostType::MiningInfo);
	printUrl(HostType::Wallet);
	printUrl(HostType::Server);

	printTargetDeadline();

	if (isLogfileUsed())
		log_system(MinerLogger::config, "Log path : %s", getConfig().getPathLogfile().toString());

	printConsolePlots();

	log_system(MinerLogger::config, "Get mining info interval : %z seconds", getConfig().getMiningInfoInterval());
	log_system(MinerLogger::config, "CPU instruction set : %s", getConfig().getCpuInstructionSet());
	log_system(MinerLogger::config, "Processor type : %s", getConfig().getProcessorType());
	
	if (getConfig().isBenchmark())
		log_warning(MinerLogger::config, "Benchmark mode activated!", );
}

void Burst::MinerConfig::printConsolePlots() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	log_system(MinerLogger::config, "Total plots size: %s", memToString(getConfig().getTotalPlotsize(), 2));
	log_system(MinerLogger::config, "Mining intensity : %z", getMiningIntensity());
	log_system(MinerLogger::config, "Max plot readers : %z", getMaxPlotReaders());
}

void Burst::MinerConfig::printUrl(HostType type) const
{
	Poco::Mutex::ScopedLock lock(mutex_);

	switch (type)
	{
	case HostType::MiningInfo: return printUrl(urlMiningInfo_, "Mininginfo URL");
	case HostType::Pool: return printUrl(urlPool_, "Submission URL");
	case HostType::Wallet: return printUrl(urlWallet_, "Wallet URL");
	case HostType::Server: return printUrl(serverUrl_, "Server URL");
	}
}

void Burst::MinerConfig::printUrl(const Url& url, const std::string& url_name)
{
	if (!url.empty())
		log_system(MinerLogger::config, "%s : %s:%hu",
			url_name ,url.getCanonical(true), url.getPort());
}

void Burst::MinerConfig::printBufferSize() const
{
	Poco::Mutex::ScopedLock lock(mutex_);

	if (maxBufferSizeMB_ == 0)
		log_system(MinerLogger::config, "Buffer Size : ~%s", memToString(getTotalPlotsize() / 4096, 0));
	else
		log_system(MinerLogger::config, "Buffer Size : %z MB", maxBufferSizeMB_);
}

template <typename T>
T getOrAdd(Poco::JSON::Object::Ptr object, const std::string& key, T defaultValue)
{
	auto json = object->get(key);

	if (json.isEmpty())
	{
		object->set(key, defaultValue);
		return defaultValue;
	}
	
	return json.convert<T>();
}

template <typename T>
T getOrAddExtract(Poco::JSON::Object::Ptr object, const std::string& key, T defaultValue)
{
	auto json = object->get(key);

	if (json.isEmpty())
		object->set(key, defaultValue);

	return json.extract<T>();
}

void Burst::MinerConfig::recalculatePlotsHash()
{
	Poco::SHA1Engine sha;
	Poco::DigestOutputStream shaStream{sha};

	for (const auto plotFile : getPlotFiles())
		shaStream << plotFile->getPath();

	shaStream << std::flush;
	plotsHash_ = Poco::SHA1Engine::digestToHex(sha.digest());

	// we remember our total plot size
	PlotSizes::set(plotsHash_, getTotalPlotsize() / 1024 / 1024 / 1024);
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

	if (!inputFileStream.is_open())
	{
		log_critical(MinerLogger::config, "Config file %s does not exist", configPath);
		return false;
	}

	configPath_ = configPath;
	plotDirs_.clear();

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

	auto checkCreateUrlFunc = [&config](Poco::JSON::Object::Ptr urlsObj, const std::string& name, Url& url,
		const std::string& defaultScheme, unsigned short defaultPort, const std::string& createUrl, bool forceInsert = false)
	{
		auto var = getOrAdd(urlsObj, name, createUrl);

		if (var.empty() && forceInsert)
		{
			url = { createUrl };
			urlsObj->set(name, url.getUri().toString());
		}
		else if (!var.empty())
		{
			url = { var, defaultScheme, defaultPort };
			urlsObj->set(name, url.getUri().toString());
		}
		else
		{
			url = { "" };
		}
	};

	// logging
	{
		Poco::JSON::Object::Ptr loggingObj = nullptr;

		if (config->has("logging"))
			loggingObj = config->get("logging").extract<Poco::JSON::Object::Ptr>();

		if (!loggingObj.isNull())
		{
			// do we need to create a logfile
			logfile_ = getOrAdd(loggingObj, "logfile", false);

			auto logOutput = getOrAdd(loggingObj, "outputType", std::string("terminal"));

			if (logOutput == "terminal")
				logOutputType_ = LogOutputType::Terminal;
			else if (logOutput == "service")
				logOutputType_ = LogOutputType::Service;
			else
				logOutputType_ = LogOutputType::Terminal;

			logUseColors_ = getOrAdd(loggingObj, "useColors", true);

			Poco::JSON::Object::Ptr progressBarObj = nullptr;

			if (loggingObj->has("progressBar"))
				progressBarObj = loggingObj->get("progressBar").extract<Poco::JSON::Object::Ptr>();

			if (progressBarObj.isNull())
			{
				progressBarObj.assign(new Poco::JSON::Object);
				progressBarObj->set("steady", true);
				progressBarObj->set("fancy", true);
				loggingObj->set("progressBar", progressBarObj);
			}
			else
			{
				steadyProgressBar_ = getOrAdd(progressBarObj, "steady", true);
				fancyProgressBar_ = getOrAdd(progressBarObj, "fancy", true);
			}

			try
			{
				auto dirLogFile = getOrAdd(loggingObj, "path", std::string(""));
				setLogDir(dirLogFile);
			}
			catch (Poco::Exception& exc)
			{
				log_fatal(MinerLogger::config, "Could not set path for log-file!\n%s", exc.displayText());
			}

			// setup logger
			for (auto& channel : MinerLogger::channelDefinitions)
			{
				if (loggingObj->has(channel.name))
					MinerLogger::setChannelPriority(channel.name, loggingObj->get(channel.name).extract<std::string>());
				else
					loggingObj->set(channel.name, MinerLogger::getChannelPriority(channel.name));
			}
		}
		// if it dont exist, we create it!
		else
		{
			loggingObj = new Poco::JSON::Object;
			loggingObj->set("path", "");
			loggingObj->set("logfile", false);
			loggingObj->set("outputType", std::string("terminal"));

			loggingObj->set("useColors", true);

			// progress bar
			{
				Poco::JSON::Object progressBarJson;
				progressBarJson.set("steady", true);
				progressBarJson.set("fancy", true);
				loggingObj->set("progressBar", progressBarJson);
			}

			for (auto& channel : MinerLogger::channelDefinitions)
				loggingObj->set(channel.name, MinerLogger::getChannelPriority(channel.name));

			config->set("logging", loggingObj);
		}

		// output
		{
			Poco::JSON::Object::Ptr outputObj;

			auto outputLoggingObj = readOutput(loggingObj->getObject("output"));

			if (!outputLoggingObj.isNull())
				outputObj = outputLoggingObj;
			else
				outputObj.assign(new Poco::JSON::Object);

			outputObj = readOutput(outputObj);

			loggingObj->set("output", outputObj);
		}
	}

	// mining
	{
		Poco::JSON::Object::Ptr miningObj;

		if (config->has("mining"))
			miningObj = config->get("mining").extract<Poco::JSON::Object::Ptr>();
		else
			miningObj = new Poco::JSON::Object;

		submission_max_retry_ = getOrAdd(miningObj, "submissionMaxRetry", 3);
		maxBufferSizeMB_ = getOrAdd(miningObj, "maxBufferSizeMB", 256);

		auto timeout = getOrAdd(miningObj, "timeout", 30);
		timeout_ = static_cast<float>(timeout);

		miningIntensity_ = getOrAdd(miningObj, "intensity", 3);
		maxPlotReaders_ = getOrAdd(miningObj, "maxPlotReaders", 0);

		walletRequestTries_ = getOrAdd(miningObj, "walletRequestTries", 5);
		walletRequestRetryWaitTime_ = getOrAdd(miningObj, "walletRequestRetryWaitTime", 3);

		// use insecure plotfiles
		useInsecurePlotfiles_ = getOrAdd(miningObj, "useInsecurePlotfiles", false);
		getMiningInfoInterval_ = getOrAdd(miningObj, "getMiningInfoInterval", 3);
		rescanEveryBlock_ = getOrAdd(miningObj, "rescanEveryBlock", true);
		
		bufferChunkCount_ = getOrAdd(miningObj, "bufferChunkCount", 8);
		wakeUpTime_ = getOrAdd(miningObj, "wakeUpTime", 0);
		
		cpuInstructionSet_ = getOrAdd(miningObj, "cpuInstructionSet", std::string("SSE2"));
		processorType_ = getOrAdd(miningObj, "processorType", std::string("CPU"));

		// benchmark
		{
			Poco::JSON::Object::Ptr benchmarkObj;

			if (miningObj->has("benchmark"))
				benchmarkObj = miningObj->get("benchmark").extract<Poco::JSON::Object::Ptr>();
			else
				benchmarkObj = new Poco::JSON::Object;

			benchmark_ = getOrAdd(benchmarkObj, "active", false);
			benchmarkInterval_ = getOrAdd(benchmarkObj, "interval", 60l);

			miningObj->set("benchmark", benchmarkObj);
		}

		// urls
		{
			Poco::JSON::Object::Ptr urlsObj;

			if (miningObj->has("urls"))
				urlsObj = miningObj->get("urls").extract<Poco::JSON::Object::Ptr>();
			else
				urlsObj = new Poco::JSON::Object;

			checkCreateUrlFunc(urlsObj, "submission", urlPool_, "http", 8124, "http://pool.burst-team.us:8124");
			checkCreateUrlFunc(urlsObj, "miningInfo", urlMiningInfo_, "http", 8124, "http://pool.burst-team.us:8124");
			checkCreateUrlFunc(urlsObj, "wallet", urlWallet_, "https", 8125, "https://wallet.burst-team.us:8128");

			if (urlMiningInfo_.empty() && !urlPool_.empty())
			{
				urlMiningInfo_ = urlPool_;
				urlsObj->set("miningInfo", urlPool_.getUri().toString());
			}

			miningObj->set("urls", urlsObj);
		}

		// plots
		{
			try
			{
				Poco::JSON::Array::Ptr arr(new Poco::JSON::Array);
				auto plotsArr = getOrAddExtract(miningObj, "plots", arr);

				auto plotsDyn = miningObj->get("plots");

				if (plotsDyn.type() == typeid(Poco::JSON::Array::Ptr))
				{
					auto plots = plotsDyn.extract<Poco::JSON::Array::Ptr>();

					for (auto& plot : *plots)
					{
						// string means sequential plot dir
						if (plot.isString())
							plotDirs_.emplace_back(new PlotDir{ plot.extract<std::string>(), PlotDir::Type::Sequential });
						// object means custom (sequential/parallel) plot dir
						else if (plot.type() == typeid(Poco::JSON::Object::Ptr))
						{
							const auto sequential = "sequential";
							const auto parallel = "parallel";

							auto plotJson = plot.extract<Poco::JSON::Object::Ptr>();
							auto type = PlotDir::Type::Sequential;

							auto typeStr = plotJson->optValue<std::string>("type", "");

							auto path = plotJson->get("path");
							
							if (path.isEmpty())
								log_error(MinerLogger::config, "Empty dir given as plot dir/file! Skipping it...");
							else if (typeStr.empty())
								log_error(MinerLogger::config, "Invalid type of plot dir/file %s! Skipping it...", path.toString());
							else if (typeStr != sequential && typeStr != parallel)
								log_error(MinerLogger::config, "Type of plot dir/file %s is invalid (%s)! Skipping it...", path.toString(), typeStr);
							else
							{
								if (typeStr == sequential)
									type = PlotDir::Type::Sequential;
								else if (typeStr == parallel)
									type = PlotDir::Type::Parallel;

								// related dirs
								if (path.type() == typeid(Poco::JSON::Array::Ptr))
								{
									auto relatedPathsJson = path.extract<Poco::JSON::Array::Ptr>();
									std::vector<std::string> relatedPaths;

									for (const auto& relatedPath : *relatedPathsJson)
									{
										if (relatedPath.isString())
											relatedPaths.emplace_back(relatedPath.extract<std::string>());
										else
											log_error(MinerLogger::config, "Invalid plot dir/file %s! Skipping it...", relatedPath.toString());
									}

									if (relatedPaths.size() == 1)
										plotDirs_.emplace_back(new PlotDir{ *relatedPaths.begin(), type });
									else if (relatedPaths.size() > 1)
										plotDirs_.emplace_back(new PlotDir{ *relatedPaths.begin(), { relatedPaths.begin() + 1, relatedPaths.end() }, type });
								}
								// single dir
								else if (path.isString())
									plotDirs_.emplace_back(new PlotDir{ path, type });
								else
									log_error(MinerLogger::config, "Invalid plot dir/file %s! Skipping it...", path.toString());
							}
						}
					}
				}
				/*else if (plotsDyn.isString())
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
				}*/
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
			recalculatePlotsHash();
		}

		// target deadline
		{
			auto targetDeadline = miningObj->get("targetDeadline");

			if (!targetDeadline.isEmpty())
			{
				// could be the raw deadline
				if (targetDeadline.isInteger())
                    targetDeadline_ = targetDeadline.convert<Poco::UInt64>();
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
				miningObj->set("targetDeadline", "0y 1m 0d 00:00:00");
				targetDeadline_ = 0;
			}
		}

		// passphrase
		{
			try
			{
				auto passphraseJson = miningObj->get("passphrase");
				Poco::JSON::Object::Ptr passphrase = nullptr;

				if (!passphraseJson.isEmpty())
					passphrase = passphraseJson.extract<Poco::JSON::Object::Ptr>();

				if (passphrase.isNull())
				{
					passphrase.assign(new Poco::JSON::Object);
					miningObj->set("passphrase", passphrase);
				}

				log_debug(MinerLogger::config, "Reading passphrase...");

				passphrase_.decrypted = getOrAdd<std::string>(passphrase, "decrypted", "");
				passphrase_.encrypted = getOrAdd<std::string>(passphrase, "encrypted", "");
				passphrase_.salt = getOrAdd<std::string>(passphrase, "salt", "");
				passphrase_.key = getOrAdd<std::string>(passphrase, "key", "");
				passphrase_.iterations = getOrAdd(passphrase, "iterations", 1000u);
				passphrase_.deleteKey = getOrAdd(passphrase, "deleteKey", false);
				passphrase_.algorithm = getOrAdd<std::string>(passphrase, "algorithm", "aes-256-cbc");

				// there is a decrypted passphrase, we need to encrypt it
				if (!passphrase_.decrypted.empty())
				{
					log_debug(MinerLogger::config, "Decrypted passphrase found, trying to encrypt...");

					passphrase_.encrypt();

					if (!passphrase_.encrypted.empty())
					{
						passphrase->set("decrypted", "");
						passphrase->set("encrypted", passphrase_.encrypted);
						passphrase->set("salt", passphrase_.salt);
						passphrase->set("key", passphrase_.key);
						passphrase->set("iterations", passphrase_.iterations);
					}
				}

				if (!passphrase_.encrypted.empty() &&
					!passphrase_.key.empty() &&
					!passphrase_.salt.empty())
				{
					log_debug(MinerLogger::config, "Encrypted passphrase found, trying to decrypt...");

					if (passphrase_.deleteKey && passphrase_.decrypted.empty())
					{
						log_debug(MinerLogger::config, "Passhrase.deleteKey == true, deleting the key...");
						passphrase->set("key", "");
					}

					passphrase_.decrypt();

					if (!passphrase_.decrypted.empty())
						log_debug(MinerLogger::config, "Passphrase decrypted!");

					// warn the user, he is possibly mining solo without knowing it
					// and sending his passphrase plain text all around the globe
					if (!passphrase_.encrypted.empty() && !passphrase_.key.empty())
						log_warning(MinerLogger::config, "WARNING! You entered a passphrase, what means you mine solo!\n"
							"This means, your passphrase is sent in plain text to 'mining.urls.submission'!\n"
							"If you don't want to mine solo, clear 'mining.passphrase.key' in your configuration file.");
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

		config->set("mining", miningObj);
	}

	//http_ = config->optValue("http", 1u);
	confirmedDeadlinesPath_ = config->optValue<std::string>("confirmed deadlines", "");

	// server credentials
	{
		auto webserverJson = config->get("webserver");

		Poco::JSON::Object::Ptr webserverObj;

		if (webserverJson.type() == typeid(Poco::JSON::Object::Ptr))
			webserverObj = webserverJson.extract<Poco::JSON::Object::Ptr>();
		else
			webserverObj.assign(new Poco::JSON::Object);

		startServer_ = getOrAdd(webserverObj, "start", true);
		checkCreateUrlFunc(webserverObj, "url", serverUrl_, "http", 8080, "http://localhost:8080", startServer_);

		// credentials
		{
			auto credentialsJson = webserverObj->get("credentials");
			Poco::JSON::Object::Ptr credentials;

			const auto userId = "user";
			const auto passId = "pass";
			
			if (!credentialsJson.isEmpty())
				credentials = credentialsJson.extract<Poco::JSON::Object::Ptr>();

			if (credentials.isNull())
			{
				credentials.assign(new Poco::JSON::Object);
				webserverObj->set("credentials", credentials);
			}

			auto pass = getOrAdd(credentials, passId, std::string{});
			auto user = getOrAdd(credentials, userId, std::string{});

			const auto getOrHash = [](auto& property, const auto& hashDelimiter,
				const auto& propertyJsonId, auto credentialsJsonObject,
				const auto& salt) {
				// user is already hashed
				if (property.size() > hashDelimiter.size() && property.substr(0, hashDelimiter.size()) == hashDelimiter)
				{
					// cut off the hash delimiter
					property = property.substr(hashDelimiter.size());
				}
				// user needs to be hashed
				else
				{
					// hash it
					property = hash_HMAC_SHA1(property, salt);

					// save it in json
					credentialsJsonObject->set(propertyJsonId, hashDelimiter + property);
				}

				return property;
			};

			if (!user.empty())
				serverUser_ = getOrHash(user, HASH_DELIMITER, userId, credentials, WebserverUserPassphrase);

			if (!pass.empty())
				serverPass_ = getOrHash(pass, HASH_DELIMITER, passId, credentials, WebserverPassPassphrase);
		}

		config->set("webserver", webserverObj);

		// warning message, when webserver is running in unsafe mode
		if ((serverUser_.empty() || serverPass_.empty()) && startServer_)
			log_warning(MinerLogger::config,
				"You are running the webserver without protecting it with an username and/or password!\n"
				"Every person with access to the webserver has FULL admin rights!");

		MinerLogger::refreshChannels();
	}
	
	if (!save(configPath_, *config))
		log_error(MinerLogger::config, "Could not save new settings!");

	return true;
}

const std::string& Burst::MinerConfig::getPath() const
{
	return configPath_;
}

std::vector<std::shared_ptr<Burst::PlotFile>> Burst::MinerConfig::getPlotFiles() const
{
	Poco::Mutex::ScopedLock lock(mutex_);

	std::vector<std::shared_ptr<Burst::PlotFile>> plotFiles;

	for (auto& plotDir : plotDirs_)
	{
		auto plotDirFiles = plotDir->getPlotfiles();

		plotFiles.insert(plotFiles.end(), plotDirFiles.begin(), plotDirFiles.end());
		
		for (const auto& relatedDir : plotDir->getRelatedDirs())
		{
			auto relatedPlotDirFiles = relatedDir->getPlotfiles();
			plotFiles.insert(plotFiles.end(), relatedPlotDirFiles.begin(), relatedPlotDirFiles.end());
		}
	}

	return plotFiles;
}

uintmax_t Burst::MinerConfig::getTotalPlotsize() const
{
	Poco::Mutex::ScopedLock lock(mutex_);

	Poco::UInt64 sum = 0;

	for (auto plotDir : plotDirs_)
	{
		sum += plotDir->getSize();
		
		for (auto relatedDir : plotDir->getRelatedDirs())
			sum += relatedDir->getSize();
	}

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
	Poco::Mutex::ScopedLock lock(mutex_);
	return timeout_;
}

Burst::Url Burst::MinerConfig::getPoolUrl() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return urlPool_;
}

Burst::Url Burst::MinerConfig::getMiningInfoUrl() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return urlMiningInfo_;
}

Burst::Url Burst::MinerConfig::getWalletUrl() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return urlWallet_;
}

size_t Burst::MinerConfig::getReceiveMaxRetry() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return receive_max_retry_;
}

size_t Burst::MinerConfig::getSendMaxRetry() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return send_max_retry_;
}

size_t Burst::MinerConfig::getSubmissionMaxRetry() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return submission_max_retry_;
}

size_t Burst::MinerConfig::getHttp() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return http_;
}

const std::string& Burst::MinerConfig::getConfirmedDeadlinesPath() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return confirmedDeadlinesPath_;
}

bool Burst::MinerConfig::getStartServer() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return startServer_;
}

Poco::UInt64 Burst::MinerConfig::getTargetDeadline() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return targetDeadline_;
}

Burst::Url Burst::MinerConfig::getServerUrl() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return serverUrl_;
}

std::unique_ptr<Poco::Net::HTTPClientSession> Burst::MinerConfig::createSession(HostType hostType) const
{
	Poco::Mutex::ScopedLock lock(mutex_);

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

Poco::JSON::Object::Ptr Burst::MinerConfig::readOutput(Poco::JSON::Object::Ptr json)
{
	if (json.isNull())
		return nullptr;

	for (auto& output : Output_Helper::Output_Names)
	{
		auto id = output.first;
		auto name = output.second;

		auto obj = json->get(name);

		if (!obj.isEmpty() && obj.isBoolean())
			MinerLogger::setOutput(id, obj.extract<bool>());
		else
			json->set(name, MinerLogger::hasOutput(id));
	}

	return json;
}

size_t Burst::MinerConfig::getMiningIntensity() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return miningIntensity_;
}

bool Burst::MinerConfig::forPlotDirs(std::function<bool(PlotDir&)> traverseFunction) const
{
	Poco::Mutex::ScopedLock lock(mutex_);

	auto success = true;

	for (auto iter = plotDirs_.begin();
		iter != plotDirs_.end() && success;
		++iter)
	{
		success = traverseFunction(**iter);
	}

	return success;
}

const std::string& Burst::MinerConfig::getPlotsHash() const
{
	Poco::Mutex::ScopedLock lock{ mutex_ };
	return plotsHash_;
}

const std::string& Burst::MinerConfig::getPassphrase() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return passphrase_.decrypted;
}

size_t Burst::MinerConfig::getMaxPlotReaders(bool real) const
{
	Poco::Mutex::ScopedLock lock(mutex_);

	// if maxPlotReaders is zero it means we have to set it to 
	// the amount of active plot dirs
	if (maxPlotReaders_ == 0 && real)
	{
		size_t notEmptyPlotdirs = 0;

		// count only the plotdirs that are not empty
		for (const auto& plotDir : plotDirs_)
			if (!plotDir->getPlotfiles().empty())
				++notEmptyPlotdirs;

		return notEmptyPlotdirs;
	}

	return maxPlotReaders_;
}

Poco::Path Burst::MinerConfig::getPathLogfile() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return pathLogfile_;
}

std::string Burst::MinerConfig::getLogDir() const
{
	return Poco::Path(getPathLogfile().parent()).toString();
}

std::string Burst::MinerConfig::getServerUser() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return serverUser_;
}

std::string Burst::MinerConfig::getServerPass() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return serverPass_;
}

void Burst::MinerConfig::setUrl(std::string url, HostType hostType)
{
	Poco::Mutex::ScopedLock lock(mutex_);
	Url* uri;

	switch (hostType)
	{
	case HostType::MiningInfo: uri = &urlMiningInfo_; break;
	case HostType::Pool: uri = &urlPool_; break;
	case HostType::Wallet: uri = &urlWallet_; break;
	default: uri = nullptr;
	}

	if (uri != nullptr)
	{
		*uri = Url(url); // change url
		//printUrl(hostType); // print url
	}
}

void Burst::MinerConfig::setBufferSize(uint64_t bufferSize)
{
	Poco::Mutex::ScopedLock lock(mutex_);
	maxBufferSizeMB_ = bufferSize;
}

void Burst::MinerConfig::setMaxSubmissionRetry(uint64_t value)
{
	Poco::Mutex::ScopedLock lock(mutex_);
	submission_max_retry_ = value;
}

void Burst::MinerConfig::setTimeout(float value)
{
	Poco::Mutex::ScopedLock lock(mutex_);
	timeout_ = value;
}

void Burst::MinerConfig::setTargetDeadline(const std::string& target_deadline)
{
	Poco::Mutex::ScopedLock lock(mutex_);
	setTargetDeadline(formatDeadline(target_deadline));
}

void Burst::MinerConfig::setTargetDeadline(uint64_t target_deadline)
{
	Poco::Mutex::ScopedLock lock(mutex_);
	targetDeadline_ = target_deadline;
}

Poco::UInt64 Burst::MinerConfig::getMaxBufferSize() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return maxBufferSizeMB_;
}

void Burst::MinerConfig::setMininigIntensity(unsigned intensity)
{
	Poco::Mutex::ScopedLock lock(mutex_);
	miningIntensity_ = intensity;
	log_system(MinerLogger::config, "", intensity);
}

void Burst::MinerConfig::setMaxPlotReaders(unsigned max_reader)
{
	Poco::Mutex::ScopedLock lock(mutex_);
	maxPlotReaders_ = max_reader;
}

size_t Burst::MinerConfig::getWalletRequestTries() const
{
	return walletRequestTries_;
}

size_t Burst::MinerConfig::getWalletRequestRetryWaitTime() const
{
	return walletRequestRetryWaitTime_;
}

size_t Burst::MinerConfig::getWakeUpTime() const
{
	return wakeUpTime_;
}

const std::string& Burst::MinerConfig::getCpuInstructionSet() const
{
	return cpuInstructionSet_;
}

const std::string& Burst::MinerConfig::getProcessorType() const
{
	return processorType_;
}

bool Burst::MinerConfig::isBenchmark() const
{
	return benchmark_;
}

long Burst::MinerConfig::getBenchmarkInterval() const
{
	return benchmarkInterval_;
}

void Burst::MinerConfig::printTargetDeadline() const
{
	if (getTargetDeadline() > 0)
		log_system(MinerLogger::config, "Target deadline : %s", deadlineFormat(getTargetDeadline()));
}

bool Burst::MinerConfig::save() const
{
	return save(configPath_);
}

bool Burst::MinerConfig::save(const std::string& path) const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	Poco::JSON::Object json;

	// logging
	{
		// logger
		Poco::JSON::Object logging;
		//
		for (auto& priority : MinerLogger::getChannelPriorities())
			logging.set(priority.first, priority.second);
		//
		// output
		Poco::JSON::Object outputs;
		//
		for (auto& output : MinerLogger::getOutput())
			outputs.set(Output_Helper::output_to_string(output.first), output.second);
		//
		logging.set("output", outputs);

		// path
		logging.set("path", getLogDir());
		
		// logfile
		logging.set("logfile", isLogfileUsed());

		// output type
		if (logOutputType_ == LogOutputType::Terminal)
			logging.set("outputType", "terminal");
		else if (logOutputType_ == LogOutputType::Service)
			logging.set("outputType", "service");
		else
			logging.set("outputType", "terminal");

		// log colors
		logging.set("useColors", isUsingLogColors());

		// progress bar
		{
			Poco::JSON::Object progressBar;

			// stready progress bar
			logging.set("steady", isSteadyProgressBar());

			// fancy progress bar
			logging.set("fancy", isFancyProgressBar());

			logging.set("progressBar", progressBar);
		}		

		json.set("logging", logging);
	}

	// mining
	{
		Poco::JSON::Object mining;

		// miningInfoInterval
		mining.set("miningInfoInterval", getMiningInfoInterval());

		// intensity
		mining.set("intensity", miningIntensity_);

		// max buffer size
		mining.set("maxBufferSizeMB", maxBufferSizeMB_);

		// max plot reader
		mining.set("maxPlotReaders", maxPlotReaders_);

		// submission max retry
		mining.set("submissionMaxRetry", submission_max_retry_);

		// target deadline
		mining.set("targetDeadline", deadlineFormat(targetDeadline_));

		// timeout
		mining.set("timeout", static_cast<Poco::UInt64>(timeout_));

		// wallet max retry wait time
		mining.set("walletRequestRetryWaitTime", walletRequestRetryWaitTime_);

		// wallet max request tries
		mining.set("walletRequestTries", walletRequestTries_);

		// insecurePlotfiles
		mining.set("useInsecurePlotfiles", useInsecurePlotfiles());

		mining.set("rescanEveryBlock", isRescanningEveryBlock());

		mining.set("bufferChunkCount", getBufferChunkCount());

		mining.set("wakeUpTime", getWakeUpTime());

		mining.set("cpuInstructionSet", getCpuInstructionSet());

		mining.set("processorType", getProcessorType());		

		// benchmark
		{
			Poco::JSON::Object benchmark;
			benchmark.set("active", isBenchmark());
			benchmark.set("interval", getBenchmarkInterval());
		}

		// passphrase
		{
			Poco::JSON::Object passphrase;

			passphrase.set("algorithm", passphrase_.algorithm);
			passphrase.set("decrypted", ""); // don't leak it
			passphrase.set("deleteKey", passphrase_.deleteKey);
			passphrase.set("encrypted", passphrase_.encrypted);
			passphrase.set("iterations", passphrase_.iterations);
			passphrase.set("encrypted", passphrase_.encrypted);

			mining.set("passphrase", passphrase);
		}

		// plots
		{
			Poco::JSON::Array plots;
			//
			for (auto& plot_dir : plotDirs_)
				plots.add(plot_dir->getPath());

			mining.set("plots", plots);
		}

		// urls
		{
			Poco::JSON::Object urls;
			//
			urls.set("miningInfo", urlMiningInfo_.getUri().toString());
			urls.set("submission", urlPool_.getUri().toString());
			urls.set("wallet", urlWallet_.getUri().toString());

			mining.set("urls", urls);
		}

		json.set("mining", mining);
	}

	// webserver
	{
		Poco::JSON::Object webserver;
		
		// credentials
		{
			Poco::JSON::Object credentials;
			//
			credentials.set("pass", serverPass_.empty() ? "" : HASH_DELIMITER + serverPass_);
			credentials.set("user", serverUser_.empty() ? "" : HASH_DELIMITER + serverUser_);
			//
			webserver.set("credentials", credentials);
		}

		// start webserver
		webserver.set("start", startServer_);

		// url
		webserver.set("url", serverUrl_.getUri().toString());

		json.set("webserver", webserver);
	}

	return save(path, json);
}

bool Burst::MinerConfig::save(const std::string& path, const Poco::JSON::Object& json)
{
	try
	{
		std::ofstream outputFileStream {path};

		std::stringstream sstr;
		json.stringify(sstr, 4);
		auto outJsonStr = sstr.str();

		Poco::replaceInPlace(outJsonStr, "\\/", "/");

		if (outputFileStream.is_open())
		{
			outputFileStream << outJsonStr << std::flush;
			outputFileStream.close();
			return true;
		}

		return false;
	}
	catch (Poco::Exception& exc)
	{
		log_exception(MinerLogger::config, exc);
		return false;
	}
	catch (std::exception& exc)
	{
		log_error(MinerLogger::config, "Exception: %s", std::string(exc.what()));
		return false;
	}
}

bool Burst::MinerConfig::addPlotDir(std::shared_ptr<PlotDir> plotDir)
{
	Poco::Mutex::ScopedLock lock(mutex_);

	// TODO: implement an existence-check before adding it
	{
		//auto iter = std::find_if(plotDirs_.begin(), plotDirs_.end(), [&](std::shared_ptr<PlotDir> element)
		//{
		//	/* TODO: it would be wiser to check the hash of both dirs
		//	 * then we would assure, that they are the same.
		//	 * BUT: the user could use very weird constellations, where
		//	 * the dir is for example inside the related dirs of another dir
		//	 */
		//	return element->getPath() == plotDir->getPath();
		//});

		//// same plotdir already in collection
		//if (iter != plotDirs_.end())
		//	return false;
	}

	plotDirs_.emplace_back(plotDir);
	return true;
}

void Burst::MinerConfig::setLogDir(const std::string& log_dir)
{
	Poco::Mutex::ScopedLock lock(mutex_);

	auto logDirAndFile = MinerLogger::setLogDir(log_dir);
	pathLogfile_ = logDirAndFile;

	if (!logfile_)
	{
		// remove the logfile
		Poco::File file{ logDirAndFile };
		
		if (file.exists())
			file.remove();

		// refresh the channels
		MinerLogger::refreshChannels();

		log_system(MinerLogger::config, "Logfile deactivated");
	}
	else if (logDirAndFile.empty())
		log_warning(MinerLogger::config, "Could not create logfile");
	else
		log_system(MinerLogger::config, "Changed logfile path to\n\t%s", logDirAndFile);
}

void Burst::MinerConfig::setGetMiningInfoInterval(size_t interval)
{
	Poco::Mutex::ScopedLock lock(mutex_);
	getMiningInfoInterval_ = interval;
}

void Burst::MinerConfig::setBufferChunkCount(size_t bufferChunkCount)
{
	Poco::Mutex::ScopedLock lock(mutex_);
	bufferChunkCount_ = bufferChunkCount;
}

bool Burst::MinerConfig::addPlotDir(const std::string& dir)
{
	return addPlotDir(std::make_shared<PlotDir>(dir, PlotDir::Type::Sequential));
}

bool Burst::MinerConfig::removePlotDir(const std::string& dir)
{
	Poco::Mutex::ScopedLock lock(mutex_);

	auto iter = std::find_if(plotDirs_.begin(), plotDirs_.end(), [&](std::shared_ptr<PlotDir> element)
	{
		// TODO: look in Burst::MinerConfig::addPlotDir
		return element->getPath() == dir;
	});

	if (iter == plotDirs_.end())
		return false;

	plotDirs_.erase(iter);
	return true;
}

const std::string& Burst::Passphrase::decrypt()
{
	decrypted = Burst::decrypt(encrypted, algorithm, key, salt, iterations);
	return decrypted;
}

const std::string& Burst::Passphrase::encrypt()
{
	encrypted = Burst::encrypt(decrypted, algorithm, key, salt, iterations);
	return encrypted;
}

bool Burst::MinerConfig::useInsecurePlotfiles() const
{
	return useInsecurePlotfiles_;
}

bool Burst::MinerConfig::isLogfileUsed() const
{
	return logfile_;
}

size_t Burst::MinerConfig::getMiningInfoInterval() const
{
	return getMiningInfoInterval_;
}

bool Burst::MinerConfig::isRescanningEveryBlock() const
{
	return rescanEveryBlock_;
}

Burst::LogOutputType Burst::MinerConfig::getLogOutputType() const
{
	return logOutputType_;
}

bool Burst::MinerConfig::isUsingLogColors() const
{
	return logUseColors_;
}

bool Burst::MinerConfig::isSteadyProgressBar() const
{
	return steadyProgressBar_;
}

bool Burst::MinerConfig::isFancyProgressBar() const
{
	return fancyProgressBar_;
}

size_t Burst::MinerConfig::getBufferChunkCount() const
{
	return bufferChunkCount_;
}

void Burst::MinerConfig::useLogfile(bool use)
{
	Poco::Mutex::ScopedLock lock(mutex_);

	if (logfile_ == use)
		return;

	// do we use a logfile?
	logfile_ = use;

	// refresh the log channels
	// because we need to open or close the filechannel
	MinerLogger::refreshChannels();
}
