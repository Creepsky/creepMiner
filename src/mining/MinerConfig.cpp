// ==========================================================================
//
// creepMiner - Burstcoin cryptocurrency CPU and GPU miner
// Copyright (C)  2016-2018 Creepsky (creepsky@gmail.com)
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
#include <Poco/FileStream.h>
#include <Poco/JSON/PrintHandler.h>
#include <Poco/StringTokenizer.h>
#include "extlibs/json.hpp"
#include <regex>
#include <Poco/Random.h>
#include <Poco/Crypto/CipherFactory.h>
#include <Poco/Crypto/CipherKey.h>
#include <Poco/Crypto/Cipher.h>

const std::string Burst::Passphrase::delimiter = "::::";

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

void Burst::MinerConfig::checkPlotOverlaps() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	Poco::UInt64 totalOverlaps = 0;

	const auto plotFiles = getPlotFiles();
	const auto numPlots = plotFiles.size();

	if (numPlots == 0)
		return;

	log_system(MinerLogger::config, "Checking local plots for overlaps...");

	for (auto iterOne = plotFiles.begin(); iterOne != plotFiles.end(); ++iterOne)
	{
		const auto& fileOne = **iterOne;

		for (auto iterTwo = iterOne + 1; iterTwo != plotFiles.end(); ++iterTwo)
		{
			const auto& fileTwo = **iterTwo;

			if (fileOne.getPath() != fileTwo.getPath() && fileOne.getAccountId() == fileTwo.getAccountId())
			{
				if (fileTwo.getNonceStart() >= fileOne.getNonceStart() && fileTwo.getNonceStart() < fileOne.getNonceStart() + fileOne.getNonces())
				{
					auto overlap = fileOne.getNonceStart() + fileOne.getNonces() - fileTwo.getNonceStart();
					if (fileTwo.getNonces() < overlap)
						overlap = fileTwo.getNonces();
					log_error(MinerLogger::miner, "%s and %s overlap by %s nonces.", fileOne.getPath(), fileTwo.getPath(), std::to_string(overlap));
					++totalOverlaps;
				}
				else if (fileOne.getNonceStart() >= fileTwo.getNonceStart() && fileOne.getNonceStart() < fileTwo.getNonceStart() + fileTwo.getNonces())
				{
					auto overlap = fileTwo.getNonceStart() + fileTwo.getNonces() - fileOne.getNonceStart();
					if (fileOne.getNonces() < overlap)
						overlap = fileOne.getNonces();
					log_error(MinerLogger::miner, "%s and %s overlap by %s nonces.", fileTwo.getPath(), fileOne.getPath(), std::to_string(overlap));
					++totalOverlaps;
				}
			}
		}
	}
	if (totalOverlaps > 0)
	{
		log_error(MinerLogger::miner, "Total overlaps found: " + std::to_string(totalOverlaps));
	}
	else
		log_system(MinerLogger::config, "No overlaps found.");
}

void Burst::MinerConfig::printConsole() const
{
	Poco::Mutex::ScopedLock lock(mutex_);

	log_system(MinerLogger::config, "Version : %s%s", Settings::project.getVersion().literal,
		Settings::project.getVersion().revision > 0
			? Poco::format(" (DEV. %u)", Settings::project.getVersion().revision)
			: std::string{});

#ifdef NDEBUG
	std::string mode = "Release";
#else
	std::string mode = "Debug";
#endif

	log_system(MinerLogger::config, "Mode : %s", mode);
	log_system(MinerLogger::config, "Build features :%s", createBuildFeatures());

	if (!getWorkerName().empty())
		log_system(MinerLogger::config, "Worker : %s", getWorkerName());

	log_system(MinerLogger::config, "Submission Max Retry : %s",
		getSubmissionMaxRetry() == 0u ? "unlimited" : std::to_string(getSubmissionMaxRetry()) + " seconds");

	printBufferSize();
	printBufferChunks();

	printUrl(HostType::Pool);
	printUrl(HostType::MiningInfo);
	printUrl(HostType::Wallet);
	printUrl(HostType::Server);

	if (!getProxyFullUrl().empty())
	{
		auto proxyConfig = getProxyConfig();
		log_system(MinerLogger::config, "Proxy URL : %s:%hu", proxyConfig.host, proxyConfig.port);
	}

	if (getConfig().getSubmitProbability() > 0.)
		printSubmitProbability();
	else
		printTargetDeadline();

	if (isLogfileUsed())
		log_system(MinerLogger::config, "Log path : %s", getConfig().getPathLogfile().toString());

	log_system(MinerLogger::config, "Database path : %s", getConfig().getDatabasePath());

	printConsolePlots();

	log_system(MinerLogger::config, "Get mining info interval : %u seconds", getConfig().getMiningInfoInterval());

	log_system(MinerLogger::config, "Processor type : %s", getConfig().getProcessorType());

	if (getConfig().getProcessorType() == "CPU")
		log_system(MinerLogger::config, "CPU instruction set : %s", getConfig().getCpuInstructionSet());
}

void Burst::MinerConfig::printConsolePlots() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	log_system(MinerLogger::config, "Total plots size: %s", memToString(getConfig().getTotalPlotsize(), 2));
	log_system(MinerLogger::config, "Mining intensity : %u", getMiningIntensity());
	log_system(MinerLogger::config, "Max plot readers : %u", getMaxPlotReaders());
}

void Burst::MinerConfig::printUrl(HostType type) const
{
	Poco::Mutex::ScopedLock lock(mutex_);

	switch (type)
	{
	case HostType::MiningInfo: return printUrl(urlMiningInfo_, "Mininginfo URL");
	case HostType::MiningInfoAlts:
		{
			for (size_t i = 0; i < urlMiningInfoAlt_.size(); ++i)
				printUrl(urlMiningInfoAlt_[i], Poco::format("Alt. Mininginfo URL %z", i));
		}
	case HostType::Pool: return printUrl(urlPool_, "Submission URL");
	case HostType::Wallet: return printUrl(urlWallet_, "Wallet URL");
	case HostType::Server: return printUrl(urlServer_, "Server URL");
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
	log_system(MinerLogger::config, "Buffer Size : %s",
		std::to_string(getMaxBufferSizeRaw()) + " (" + memToString(getMaxBufferSize(), 0) + ")");
}

void Burst::MinerConfig::printBufferChunks() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	log_system(MinerLogger::config, "Buffer Chunks : %s (%s each)", std::to_string(getBufferChunkCount()),
		memToString(getMaxBufferSize() / getBufferChunkCount(), 0));
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
	{
		object->set(key, defaultValue);
		return defaultValue;
	}

	return json.extract<T>();
}

void Burst::MinerConfig::recalculatePlotsHash()
{
	Poco::SHA1Engine sha;
	Poco::DigestOutputStream shaStream{sha};

	for (const auto& plotFile : getPlotFiles())
		shaStream << plotFile->getPath();

	shaStream << std::flush;
	plotsHash_ = Poco::SHA1Engine::digestToHex(sha.digest());

	// we remember our total plot size
	PlotSizes::set(Poco::Net::IPAddress{"127.0.0.1"}, getTotalPlotsize(), true);
}

Burst::ReadConfigFileResult Burst::MinerConfig::readConfigFile(const std::string& configPath)
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
		return ReadConfigFileResult::Error;
	}

	if (!inputFileStream.is_open())
		return ReadConfigFileResult::NotFound;

	configPath_ = configPath;
	plotDirs_.clear();

	Poco::JSON::Parser parser;
	Poco::JSON::Object::Ptr config;
	std::stringstream jsonValidationStream;

	parser.setAllowComments(true);

	const std::string jsonStr((std::istreambuf_iterator<char>(inputFileStream)),
		std::istreambuf_iterator<char>());

	inputFileStream.close();

	try
	{
		// validate the syntax
		nlohmann::json::parse(jsonStr);

		// parse the config
		config = parser.parse(jsonStr).extract<Poco::JSON::Object::Ptr>();
	}
	catch (nlohmann::json::parse_error& exc)
	{
		const auto slice = 256;
		auto startByte = std::max(0, static_cast<int>(exc.byte) - slice);
		auto endByte = std::min(exc.byte + slice, jsonStr.size());

		if (startByte < 5)
			startByte = 0;

		if (endByte > jsonStr.size() - 5)
			endByte = jsonStr.size();

		const auto errorColor = MinerLogger::getTextTypeColor(TextType::Error);

		auto printBlock = Console::print();
		printBlock.addTime() << ": " << errorColor << "There is an error in the config file!";
		printBlock.nextLine();
		printBlock.resetColor();

		if (startByte > 0)
			printBlock << "...";

		printBlock << jsonStr.substr(startByte, exc.byte - startByte)
			<< errorColor << " <-- error somewhere here";
		printBlock.resetColor() << jsonStr.substr(exc.byte, endByte - exc.byte);

		if (endByte < jsonStr.size())
			printBlock << "...";

		printBlock.nextLine();
		return ReadConfigFileResult::Invalid;
	}
	catch (Poco::Exception& exc)
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

		return ReadConfigFileResult::Invalid;
	}

	const auto checkCreateUrlFunc = [](Poco::JSON::Object::Ptr urlsObj, const std::string& name, Url& url,
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
			logfile_ = getOrAdd(loggingObj, "logfile", true);

			const auto logOutput = getOrAdd(loggingObj, "outputType", std::string("terminal"));

			if (logOutput == "terminal")
				logOutputType_ = LogOutputType::Terminal;
			else if (logOutput == "service")
				logOutputType_ = LogOutputType::Service;
			else
				logOutputType_ = LogOutputType::Terminal;

			logUseColors_ = getOrAdd(loggingObj, "useColors", true);
			verboseLogging_ = getOrAdd(loggingObj, "verbose", true);

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
				const auto dirLogFile = getOrAdd(loggingObj, "path", std::string(""));
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
			miningObj.assign(new Poco::JSON::Object);

		submissionMaxRetry_ = getOrAdd(miningObj, "submissionMaxRetry", 10);
		maxBufferSizeMb_ = getOrAdd(miningObj, "maxBufferSizeMB", 0u);

		const auto timeout = getOrAdd(miningObj, "timeout", 30);
		timeout_ = static_cast<float>(timeout);

		miningIntensity_ = getOrAdd(miningObj, "intensity", 0);
		maxPlotReaders_ = getOrAdd(miningObj, "maxPlotReaders", 0);

		walletRequestTries_ = getOrAdd(miningObj, "walletRequestTries", 5);
		walletRequestRetryWaitTime_ = getOrAdd(miningObj, "walletRequestRetryWaitTime", 3);

		// use insecure plotfiles
		useInsecurePlotfiles_ = getOrAdd(miningObj, "useInsecurePlotfiles", false);
		getMiningInfoInterval_ = getOrAdd(miningObj, "getMiningInfoInterval", 3);
		rescanEveryBlock_ = getOrAdd(miningObj, "rescanEveryBlock", false);

		bufferChunkCount_ = getOrAdd(miningObj, "bufferChunkCount", 8);
		wakeUpTime_ = getOrAdd(miningObj, "wakeUpTime", 0);

		cpuInstructionSet_ = Poco::toUpper(getOrAdd(miningObj, "cpuInstructionSet", std::string("AUTO")));
		cpuInstructionSet_ = Poco::trim(cpuInstructionSet_);

		databasePath_ = getOrAdd(miningObj, "databasePath", std::string("data.db"));
		workerName_ = getOrAdd(miningObj, "workerName", std::string{});
		poc2StartBlock_ = getOrAdd(miningObj, "poc2StartBlock", 502000);

		// Check if CPU instruction set was wrongly configured. Auto detect in case of wrong settings.
		if ((((cpuInstructionSet_ == "AVX2") && !cpuHasInstructionSet(CpuInstructionSet::Avx2))
			|| ((cpuInstructionSet_ == "AVX") && !cpuHasInstructionSet(CpuInstructionSet::Avx))
			|| ((cpuInstructionSet_ == "SSE4") && !cpuHasInstructionSet(CpuInstructionSet::Sse4)))
			|| ((cpuInstructionSet_ != "AVX2") || (cpuInstructionSet_ != "AVX")
				|| (cpuInstructionSet_ != "SSE4")|| (cpuInstructionSet_ != "SSE2"))) {
			log_warning(MinerLogger::config, "Configured instruction set is not supported by CPU!");
			cpuInstructionSet_ = "AUTO";
		}
		// auto detect the max. cpu instruction set
		if (cpuInstructionSet_ == "AUTO")
		{
			if (cpuHasInstructionSet(CpuInstructionSet::Avx2))
				cpuInstructionSet_ = "AVX2";
			else if (cpuHasInstructionSet(CpuInstructionSet::Avx))
				cpuInstructionSet_ = "AVX";
			else if (cpuHasInstructionSet(CpuInstructionSet::Sse4))
				cpuInstructionSet_ = "SSE4";
			else
				cpuInstructionSet_ = "SSE2";
		}

		Settings::setCpuInstructionSet(cpuInstructionSet_);

		std::string defaultProcessorType;

		if (Settings::cuda)
			defaultProcessorType = "CUDA";
		else if (Settings::openCl)
			defaultProcessorType = "OPENCL";
		else
			defaultProcessorType = "CPU";

		processorType_ = getOrAdd(miningObj, "processorType", defaultProcessorType);

		gpuPlatform_ = getOrAdd(miningObj, "gpuPlatform", 0u);
		gpuDevice_ = getOrAdd(miningObj, "gpuDevice", 0u);

		// urls
		{
			const auto checkCreateUrlArrayFunc = [&checkCreateUrlFunc](Poco::JSON::Object::Ptr urlsObj, const std::string& name,
			                                                           Url& url,
			                                                           std::vector<Url>& altUrls,
			                                                           const std::string& defaultScheme,
			                                                           unsigned short defaultPort,
			                                                           const std::string& createUrl)
			{
				if (urlsObj->isArray(name))
				{
					const auto urls = urlsObj->getArray(name);

					for (unsigned i = 0; i < urls->size(); ++i)
					{
						if (i == 0)
							url = Url{urls->getElement<std::string>(i)};
						else
							altUrls.emplace_back(urls->getElement<std::string>(i));
					}
				}
				else
				{
					checkCreateUrlFunc(urlsObj, name, url, defaultScheme, defaultPort, createUrl);
				}
			};

			Poco::JSON::Object::Ptr urlsObj;

			if (miningObj->has("urls"))
				urlsObj = miningObj->get("urls").extract<Poco::JSON::Object::Ptr>();
			else
				urlsObj = new Poco::JSON::Object;

			checkCreateUrlArrayFunc(urlsObj, "submission", urlPool_, urlPoolAlt_, "https", 443,
			                        "https://pool.creepminer.net");
			checkCreateUrlArrayFunc(urlsObj, "miningInfo", urlMiningInfo_, urlMiningInfoAlt_, "https", 443,
			                        "https://pool.creepminer.net");
			checkCreateUrlFunc(urlsObj, "wallet", urlWallet_, "https", 443, "https://wallet.creepminer.net");

			if (urlMiningInfo_.empty() && !urlPool_.empty())
			{
				urlMiningInfo_ = urlPool_;
				urlsObj->set("miningInfo", urlPool_.getUri().toString());
			}

			const auto urlProxy = getOrAdd(urlsObj, "proxy", std::string{});

			if (!urlProxy.empty())
			{
				try
				{
					std::smatch match;

					const auto extractEncrypted = [](const std::string& property)
					{
						auto encrypted = Passphrase::fromString(property);

						if (encrypted.isPlainText())
						{
							if (encrypted.algorithm.empty())
								encrypted.algorithm = "aes-256-cbc";

							encrypted.encrypt();
						}

						return encrypted;
					};

					// user:password@host:port
					if (std::regex_match(urlProxy, match, std::regex{"^(.+)(&|:)(.+)@(.+):(.+)$"}))
					{
						proxyUser_ = extractEncrypted(match[1].str());
						proxyPassword_ = extractEncrypted(match[3].str());
						proxyIp_ = match[4].str();
						proxyPort_ = static_cast<Poco::UInt16>(Poco::NumberParser::parse(match[5].str()));
					}
					// user@host:port
					else if (std::regex_match(urlProxy, match, std::regex{"^(.+)@(.+):(.+)$"}))
					{
						proxyUser_ = extractEncrypted(match[1].str());
						proxyIp_ = match[2].str();
						proxyPort_ = static_cast<Poco::UInt16>(Poco::NumberParser::parse(match[3].str()));
					}
					// host:port
					else if (std::regex_match(urlProxy, match, std::regex{"^(.+):(.+)$"}))
					{
						proxyIp_ = match[1].str();
						proxyPort_ = static_cast<Poco::UInt16>(Poco::NumberParser::parse(match[2].str()));
					}
					// host
					else if (std::regex_match(urlProxy, match, std::regex{"^(.+)$"}))
					{
						proxyIp_ = match[1].str();
						proxyPort_ = 8080;
						log_warning(MinerLogger::config, "Your proxy URL has no port definition, 8080 is assumed.\n"
							"Please use the format [user:password@]host:port");
					}
					else
					{
						log_error(MinerLogger::config, "Invalid proxy URL format, please use [user:password@]host:port");
					}
				}
				catch (const Poco::Exception& e)
				{
					log_error(MinerLogger::config, "Could not parse the proxy URL: %s", e.displayText());
				}
			}

			urlsObj->set("proxy", getProxyFullUrl());
			miningObj->set("urls", urlsObj);
		}

		// plots
		{
			try
			{
				const Poco::JSON::Array::Ptr arr(new Poco::JSON::Array);
				auto plotsArr = getOrAddExtract(miningObj, "plots", arr);

				auto plotsDyn = miningObj->get("plots");

				if (plotsDyn.type() == typeid(Poco::JSON::Array::Ptr))
				{
					auto plots = plotsDyn.extract<Poco::JSON::Array::Ptr>();

					for (auto& plot : *plots)
					{
						try
						{
							// string means sequential plot dir
							if (plot.isString())
								addPlotDir(std::make_shared<PlotDir>(plot.extract<std::string>(), PlotDir::Type::Sequential));
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
						catch (const Poco::Exception& e)
						{
							log_warning(MinerLogger::config, "Error while adding the plotdir/file: %s", e.message());
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


		// max historical data
		{
		auto maxHistoricalBlocks = miningObj->get("maxHistoricalBlocks");

		if (!maxHistoricalBlocks.isEmpty())
		{
			setMaxHistoricalBlocks(maxHistoricalBlocks.convert<Poco::UInt64>());
		}
		else
		{
			miningObj->set("maxHistoricalBlocks", 360);
			setMaxHistoricalBlocks(360);
		}
		}

		// submit probability
		{
			auto submitProbability = miningObj->get("submitProbability");

			if (!submitProbability.isEmpty())
			{
				setSubmitProbability( static_cast<double>(submitProbability) );
			}
			else
			{
				miningObj->set("submitProbability", 0.999);
				setSubmitProbability( 0.999 );
			}
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
				miningObj->set("targetDeadline", "0y 0m 0d 00:00:00");
				targetDeadline_ = 0;
			}
		}

		// passphrase
		{
			try
			{
				if (miningObj->isObject("passphrase"))
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

					if (passphrase->has("decrypted") || passphrase->has("encrypted"))
					{
						passphrase_.decrypted = getOrAdd<std::string>(passphrase, "decrypted", "");
						passphrase_.encrypted = getOrAdd<std::string>(passphrase, "encrypted", "");
						passphrase_.salt = getOrAdd<std::string>(passphrase, "salt", "");
						passphrase_.key = getOrAdd<std::string>(passphrase, "key", "");
						passphrase_.iterations = getOrAdd(passphrase, "iterations", 1000u);
						passphrase_.deleteKey = getOrAdd(passphrase, "deleteKey", true);
						passphrase_.algorithm = getOrAdd<std::string>(passphrase, "algorithm", "aes-256-cbc");
					}
				}
				else
				{
					passphrase_ = Passphrase::fromString(miningObj->getValue<std::string>("passphrase"));
				}

				// there is a decrypted passphrase, we need to encrypt it
				if (!passphrase_.decrypted.empty())
				{
					log_debug(MinerLogger::config, "Decrypted passphrase found, trying to encrypt...");

					if (passphrase_.algorithm.empty())
						passphrase_.algorithm = "aes-256-cbc";

					passphrase_.encrypt();
				}

				if (!passphrase_.encrypted.empty() &&
					!passphrase_.key.empty() &&
					!passphrase_.salt.empty())
				{
					log_debug(MinerLogger::config, "Encrypted passphrase found, trying to decrypt...");

					passphrase_.decrypt();

					if (passphrase_.deleteKey && passphrase_.decrypted.empty())
					{
						log_debug(MinerLogger::config, "Passhrase.deleteKey == true, deleting the key...");
						passphrase_.key.clear();
					}

					if (!passphrase_.decrypted.empty())
						log_debug(MinerLogger::config, "Passphrase decrypted!");

					// warn the user, he is possibly mining solo without knowing it
					// and sending his passphrase plain text all around the globe
					if (!passphrase_.encrypted.empty() && !passphrase_.key.empty())
						log_warning(MinerLogger::config, "WARNING! You entered a passphrase, what means you mine solo!\n"
							"This means, your passphrase is sent in plain text to 'mining.urls.submission'!\n"
							"If you don't want to mine solo, clear 'mining.passphrase' in your configuration file.");
				}

				if (passphrase_.empty())
					miningObj->set("passphrase", "");
				else
					miningObj->set("passphrase", passphrase_.toString());
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
		checkCreateUrlFunc(webserverObj, "url", urlServer_, "http", 8124, "http://0.0.0.0:8124", startServer_);
		maxConnectionsQueued_ = getOrAdd(webserverObj, "connectionQueue", 64u);
		maxConnectionsActive_ = getOrAdd(webserverObj, "activeConnections", 16u);
		cumulatePlotsizes_ = getOrAdd(webserverObj, "cumulatePlotsizes", true);
		minerNameForwarding_ = getOrAdd(webserverObj, "forwardMinerNames", true);
		calculateEveryDeadline_ = getOrAdd(webserverObj, "calculateEveryDeadline", false);

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

			auto serverPass = getOrAdd(credentials, passId, std::string{});
			auto serverUser = getOrAdd(credentials, userId, std::string{});

			const auto getOrHash = [&](std::string& property,
			                           auto propertyJsonId, auto credentialsJsonObject,
			                           const auto& defaultSalt)
			{
				auto encryption = Passphrase::fromString(property);

				if (encryption.isPlainText())
				{
					encryption.encrypt();
				}
				else if (encryption.salt.empty())
				{
					encryption.salt = defaultSalt;
				}

				credentialsJsonObject->set(propertyJsonId, encryption.toString());

				return encryption;
			};

			if (!serverUser.empty())
				serverUser_ = getOrHash(serverUser, userId, credentials, "ms7zKm7QjsSOQEP13wHAWnraSp7yP7YSQdPzAjvO");

			if (!serverPass.empty())
				serverPass_ = getOrHash(serverPass, passId, credentials, "CAAwj6RTQqXZGxbNjLVqr5FwAqT7GM9Y1wppNLRp");
		}

		// forwarding
		{
			const Poco::JSON::Array::Ptr arr(new Poco::JSON::Array);
			auto forwardUrls = getOrAddExtract(webserverObj, "forwardUrls", arr);

			for (const auto& url : *forwardUrls)
			{
				try
				{
					forwardingWhitelist_.emplace_back(url.extract<std::string>());
				}
				catch (...)
				{
					log_error(MinerLogger::config, "Invalid forwarding rule in config: %s", url.toString());
				}
			}
		}

		// certificate
		{
			auto certificateJson = webserverObj->get("certificate");
			Poco::JSON::Object::Ptr certificate;

			if (!certificateJson.isEmpty())
				certificate = certificateJson.extract<Poco::JSON::Object::Ptr>();

			if (certificate.isNull())
			{
				certificate.assign(new Poco::JSON::Object);
				certificate->set("path", "");
				certificate->set("pass", "");
				webserverObj->set("certificate", certificate);
			}

			serverCertificatePath_ = getOrAdd(certificate, "path", std::string{});
			serverCertificatePass_ = getOrAdd(certificate, "pass", std::string{});

			if (!serverCertificatePass_.empty())
			{
				auto encryption = Passphrase::fromString(serverCertificatePass_);

				if (encryption.algorithm.empty())
					encryption.algorithm = "aes-256-cbc";

				if (encryption.isPlainText())
					encryption.encrypt();

				certificate->set("pass", encryption.toString());
			}
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

	return ReadConfigFileResult::Ok;
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

	for (const auto& plotDir : plotDirs_)
	{
		sum += plotDir->getSize();

		for (const auto& relatedDir : plotDir->getRelatedDirs())
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

double Burst::MinerConfig::getTargetDLFactor() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return targetDlFactor_;
}

double Burst::MinerConfig::getDeadlinePerformanceFac() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return deadlinePerformanceFac_;
}

double Burst::MinerConfig::getSubmitProbability() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return submitProbability_;
}

Burst::Url Burst::MinerConfig::getPoolUrl() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return urlPool_;
}

const std::vector<Burst::Url>& Burst::MinerConfig::getPoolUrlAlt() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return urlPoolAlt_;
}

Burst::Url Burst::MinerConfig::getMiningInfoUrl() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return urlMiningInfo_;
}

const std::vector<Burst::Url>& Burst::MinerConfig::getMiningInfoUrlAlt() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return urlMiningInfoAlt_;
}

Burst::Url Burst::MinerConfig::getWalletUrl() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return urlWallet_;
}

std::string Burst::MinerConfig::getProxyFullUrl() const
{
	Poco::Mutex::ScopedLock lock{mutex_};

	if (proxyUser_.empty())
		return "";

	std::stringstream sstream;

	if (!proxyUser_.empty())
	{
		sstream << proxyUser_.toString();
		
		if (!proxyPassword_.empty())
			sstream << '&' << proxyPassword_.toString();

		sstream << '@';
	}

	sstream << proxyIp_ << ':' << proxyPort_;

	return sstream.str(); 
}

Poco::Net::HTTPClientSession::ProxyConfig Burst::MinerConfig::getProxyConfig() const
{
	Poco::Net::HTTPClientSession::ProxyConfig proxyConfig;

	if (proxyIp_.empty() || proxyPort_ == 0)
		return proxyConfig;

	proxyConfig.host = proxyIp_;
	proxyConfig.port = proxyPort_;

	if (!proxyUser_.empty())
	{
		auto passphrase = proxyUser_;
		proxyConfig.username = passphrase.decrypt();
	}

	if (!proxyPassword_.empty())
	{
		auto passphrase = proxyPassword_;
		proxyConfig.password = passphrase.decrypt();
	}

	return proxyConfig;
}

unsigned Burst::MinerConfig::getReceiveMaxRetry() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return receiveMaxRetry_;
}

unsigned Burst::MinerConfig::getSendMaxRetry() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return sendMaxRetry_;
}

unsigned Burst::MinerConfig::getSubmissionMaxRetry() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return submissionMaxRetry_;
}

unsigned Burst::MinerConfig::getHttp() const
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

const std::string& Burst::MinerConfig::getServerCertificatePath() const
{
	return serverCertificatePath_;
}

const std::string& Burst::MinerConfig::getServerCertificatePass() const
{
	return serverCertificatePass_;
}

const std::string& Burst::MinerConfig::getDatabasePath() const
{
	return databasePath_;
}

const std::string& Burst::MinerConfig::getWorkerName() const
{
	return workerName_;
}

Poco::UInt64 Burst::MinerConfig::getTargetDeadline(TargetDeadlineType type) const
{
	Poco::Mutex::ScopedLock lock(mutex_);

	switch (type)
	{
	case TargetDeadlineType::Pool:
		return targetDeadlinePool_;
	case TargetDeadlineType::Local:
		return targetDeadline_;
	case TargetDeadlineType::Combined:
	{
		auto targetDeadline = targetDeadlinePool_;
		const auto manualTargetDeadline = targetDeadline_;

		if (targetDeadline == 0)
			targetDeadline = manualTargetDeadline;
		else if (targetDeadline > manualTargetDeadline &&
			manualTargetDeadline > 0)
			targetDeadline = manualTargetDeadline;

		return targetDeadline;
	}
	default:
		return 0;
	}
}

Burst::Url Burst::MinerConfig::getServerUrl() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return urlServer_;
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

	if (url == nullptr)
		return nullptr;

	return createSession(*url);
}

std::unique_ptr<Poco::Net::HTTPClientSession> Burst::MinerConfig::createSession(const Url& url) const
{
	if (!url.empty())
	{
		auto session = url.createSession();
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

	for (auto& output : OutputHelper::outputNames)
	{
		const auto id = output.first;
		const auto name = output.second;

		auto obj = json->get(name);

		if (!obj.isEmpty() && obj.isBoolean())
			MinerLogger::setOutput(id, obj.extract<bool>());
		else
			json->set(name, MinerLogger::hasOutput(id));
	}

	return json;
}

unsigned Burst::MinerConfig::getMiningIntensity(bool real) const
{
	Poco::Mutex::ScopedLock lock(mutex_);

	if (!real)
		return miningIntensity_;

	if (miningIntensity_ == 0)
		return getMaxPlotReaders(true);

	return miningIntensity_;
}

bool Burst::MinerConfig::forPlotDirs(const std::function<bool(PlotDir&)>& traverseFunction) const
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

//const std::string& Burst::MinerConfig::getPlotsHash() const
//{
//	Poco::Mutex::ScopedLock lock{ mutex_ };
//	return plotsHash_;
//}

const std::string& Burst::MinerConfig::getPassphrase() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return passphrase_.decrypted;
}

unsigned Burst::MinerConfig::getMaxPlotReaders(bool real) const
{
	Poco::Mutex::ScopedLock lock(mutex_);

	// if maxPlotReaders is zero it means we have to set it to
	// the amount of active plot dirs
	if (maxPlotReaders_ == 0 && real)
	{
		unsigned notEmptyPlotdirs = 0;

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

const Burst::Passphrase& Burst::MinerConfig::getServerUser() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return serverUser_;
}

const Burst::Passphrase& Burst::MinerConfig::getServerPass() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return serverPass_;
}

void Burst::MinerConfig::setUrl(const std::string& url, const HostType hostType)
{
	Poco::Mutex::ScopedLock lock(mutex_);
	Url* uri;

	switch (hostType)
	{
	case HostType::MiningInfo: uri = &urlMiningInfo_; break;
	case HostType::Pool: uri = &urlPool_; break;
	case HostType::Wallet: uri = &urlWallet_; break;
	case HostType::Server: uri = &urlServer_; break;
	default: uri = nullptr;
	}

	if (uri != nullptr)
	{
		*uri = Url(url); // change url
		//printUrl(hostType); // print url
	}
}

void Burst::MinerConfig::setBufferSize(Poco::UInt64 bufferSize)
{
	Poco::Mutex::ScopedLock lock(mutex_);
	maxBufferSizeMb_ = bufferSize;
}

void Burst::MinerConfig::setMaxSubmissionRetry(unsigned value)
{
	Poco::Mutex::ScopedLock lock(mutex_);
	submissionMaxRetry_ = value;
}

void Burst::MinerConfig::setTimeout(float value)
{
	Poco::Mutex::ScopedLock lock(mutex_);
	timeout_ = value;
}

void Burst::MinerConfig::setMaxHistoricalBlocks(Poco::UInt64 maxHistData)
{
	if (maxHistData < 30)
		maxHistoricalBlocks_ = 30;
	else if (maxHistData > 3600)
		maxHistoricalBlocks_ = 3600;
	else
		maxHistoricalBlocks_ = maxHistData;
}

void Burst::MinerConfig::setSubmitProbability(double subP)
{
	if (subP < 0)
		submitProbability_ = 0;
	else if (subP >=0.999999)
		submitProbability_ = 0.999999;
	else
		submitProbability_ = subP;

	if (submitProbability_ > 0)
		deadlinePerformanceFac_ = (1.0 + (1.0 - subP) / subP * log(1.0 - subP)) * 240.0;
	else
		deadlinePerformanceFac_ = 1.0 * 240.0;

	targetDlFactor_ = -log(1.0 - submitProbability_) * 240.0;
}


void Burst::MinerConfig::setTargetDeadline(const std::string& target_deadline, TargetDeadlineType type)
{
	Poco::Mutex::ScopedLock lock(mutex_);
	setTargetDeadline(formatDeadline(target_deadline), type);
}

void Burst::MinerConfig::setTargetDeadline(Poco::UInt64 target_deadline, TargetDeadlineType type)
{
	Poco::Mutex::ScopedLock lock(mutex_);

	if (type == TargetDeadlineType::Local)
		targetDeadline_ = target_deadline;
	else if (type == TargetDeadlineType::Pool)
		targetDeadlinePool_ = target_deadline;
}

Poco::UInt64 Burst::MinerConfig::getMaxBufferSize() const
{
	Poco::Mutex::ScopedLock lock(mutex_);

	if (maxBufferSizeMb_ == 0)
	{
		Poco::UInt64 maxSize = 0;

		for (const auto& plot : getPlotFiles())
			if (plot->getStaggerScoopBytes() > maxSize)
				maxSize = plot->getStaggerScoopBytes();

		return maxSize * getBufferChunkCount();
	}

	return maxBufferSizeMb_ * 1024 * 1024;
}

Poco::UInt64 Burst::MinerConfig::getMaxBufferSizeRaw() const
{
	return maxBufferSizeMb_;
}

Poco::UInt64 Burst::MinerConfig::getMaxHistoricalBlocks() const
{
	Poco::Mutex::ScopedLock lock(mutex_);
	return maxHistoricalBlocks_;
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

unsigned Burst::MinerConfig::getWalletRequestTries() const
{
	return walletRequestTries_;
}

unsigned Burst::MinerConfig::getWalletRequestRetryWaitTime() const
{
	return walletRequestRetryWaitTime_;
}

unsigned Burst::MinerConfig::getWakeUpTime() const
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

unsigned Burst::MinerConfig::getGpuPlatform() const
{
	return gpuPlatform_;
}

unsigned Burst::MinerConfig::getGpuDevice() const
{
	return gpuDevice_;
}

void Burst::MinerConfig::printTargetDeadline() const
{
	if (getTargetDeadline() > 0)
		log_system(MinerLogger::config, "Target deadline : %s", deadlineFormat(getTargetDeadline(TargetDeadlineType::Local)));
}

void Burst::MinerConfig::printSubmitProbability() const
{
		log_system(MinerLogger::config, "Submit probability : %s", numberToString(getSubmitProbability()));
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

		for (auto& priority : MinerLogger::getChannelPriorities())
			logging.set(priority.first, priority.second);

		// output
		Poco::JSON::Object outputs;

		for (auto& output : MinerLogger::getOutput())
			outputs.set(OutputHelper::outputToString(output.first), output.second);

		logging.set("output", outputs);

		logging.set("path", getLogDir());
		logging.set("logfile", isLogfileUsed());
		logging.set("useColors", isUsingLogColors());

		// output type
		if (logOutputType_ == LogOutputType::Terminal)
			logging.set("outputType", "terminal");
		else if (logOutputType_ == LogOutputType::Service)
			logging.set("outputType", "service");
		else
			logging.set("outputType", "terminal");


		// progress bar
		{
			Poco::JSON::Object progressBar;
			progressBar.set("steady", isSteadyProgressBar());
			progressBar.set("fancy", isFancyProgressBar());
			logging.set("progressBar", progressBar);
		}

		json.set("logging", logging);
	}

	// mining
	{
		Poco::JSON::Object mining;

		// miningInfoInterval
		mining.set("getMiningInfoInterval", getMiningInfoInterval());
		mining.set("intensity", miningIntensity_);
		mining.set("maxBufferSizeMB", maxBufferSizeMb_);
		mining.set("maxPlotReaders", maxPlotReaders_);
		mining.set("submissionMaxRetry", submissionMaxRetry_);
		mining.set("maxHistoricalBlocks", maxHistoricalBlocks_);
		mining.set("submitProbability", submitProbability_);
		mining.set("targetDeadline", deadlineFormat(targetDeadline_));
		mining.set("timeout", static_cast<Poco::UInt64>(timeout_));
		mining.set("walletRequestRetryWaitTime", walletRequestRetryWaitTime_);
		mining.set("walletRequestTries", walletRequestTries_);
		mining.set("useInsecurePlotfiles", useInsecurePlotfiles());
		mining.set("rescanEveryBlock", isRescanningEveryBlock());
		mining.set("bufferChunkCount", getBufferChunkCount());
		mining.set("wakeUpTime", getWakeUpTime());
		mining.set("cpuInstructionSet", getCpuInstructionSet());
		mining.set("processorType", getProcessorType());
		mining.set("gpuDevice", getGpuDevice());
		mining.set("gpuPlatform", getGpuPlatform());
		mining.set("databasePath", getDatabasePath());
		mining.set("workerName", getWorkerName());

		// passphrase
		{
			Poco::JSON::Object passphrase;
			passphrase.set("algorithm", passphrase_.algorithm);
			passphrase.set("decrypted", ""); // don't leak it
			passphrase.set("deleteKey", passphrase_.deleteKey);
			passphrase.set("encrypted", passphrase_.encrypted);
			passphrase.set("iterations", passphrase_.iterations);
			passphrase.set("encrypted", passphrase_.encrypted);
			passphrase.set("key", passphrase_.deleteKey ? "" : passphrase_.key);
			passphrase.set("salt", passphrase_.salt);
			mining.set("passphrase", passphrase);
		}

		// plots
		{
			Poco::JSON::Array plots;
			for (auto& plotDir : plotDirs_)
				plots.add(plotDir->getPath());
			mining.set("plots", plots);
		}

		// urls
		{
			Poco::JSON::Object urls;
			urls.set("miningInfo", urlMiningInfo_.getUri().toString());
			urls.set("submission", urlPool_.getUri().toString());
			urls.set("wallet", urlWallet_.getUri().toString());
			urls.set("proxy", getProxyFullUrl());
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
			credentials.set("pass", serverPass_.empty() ? "" : serverPass_.toString());
			credentials.set("user", serverUser_.empty() ? "" : serverUser_.toString());
			webserver.set("credentials", credentials);
		}

		webserver.set("start", startServer_);
		webserver.set("activeConnections", getMaxConnectionsActive());
		webserver.set("calculateEveryDeadline", isCalculatingEveryDeadline());
		webserver.set("connectionQueue", getMaxConnectionsQueued());
		webserver.set("cumulatePlotsizes", isCumulatingPlotsizes());
		webserver.set("forwardMinerNames", isForwardingMinerName());
		webserver.set("url", urlServer_.getUri().toString());

		// forwardUrls
		{
			Poco::JSON::Array forwardUrls;
			for (const auto& forward : getForwardingWhitelist())
				forwardUrls.add(forward);
			webserver.set("forwardUrls", forwardUrls);
		}

		// certificate
		{
			Poco::JSON::Object certificate;
			certificate.set("path", getServerCertificatePath());
			certificate.set("pass", getServerCertificatePass());
			webserver.set("certificate", certificate);
		}

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

bool Burst::MinerConfig::isCalculatingEveryDeadline() const
{
	return calculateEveryDeadline_;
}

bool Burst::MinerConfig::isForwardingEverything() const
{
	return forwardingWhitelist_.empty();
}

const std::vector<std::string>& Burst::MinerConfig::getForwardingWhitelist() const
{
	return forwardingWhitelist_;
}

bool Burst::MinerConfig::isCumulatingPlotsizes() const
{
	return cumulatePlotsizes_;
}

unsigned Burst::MinerConfig::getMaxConnectionsQueued() const
{
	return maxConnectionsQueued_;
}

unsigned Burst::MinerConfig::getMaxConnectionsActive() const
{
	return maxConnectionsActive_;
}

bool Burst::MinerConfig::addPlotDir(std::shared_ptr<PlotDir> plotDir)
{
	Poco::Mutex::ScopedLock lock(mutex_);

	const auto iter = std::find_if(plotDirs_.begin(), plotDirs_.end(), [&](const std::shared_ptr<PlotDir>& element)
	{
		return element->getPath() == plotDir->getPath() ||
			element->getHash() == plotDir->getHash();
	});

	// same plotdir already in collection
	if (iter != plotDirs_.end())
		throw Poco::Exception{Poco::format("The plotfile/dir %s already exists!", plotDir->getPath())};

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

		log_debug(MinerLogger::config, "Logfile deactivated");
	}
	else if (logDirAndFile.empty())
		log_warning(MinerLogger::config, "Could not create logfile");
	else
		log_system(MinerLogger::config, "Changed logfile path to\n\t%s", logDirAndFile);
}

void Burst::MinerConfig::setGetMiningInfoInterval(unsigned interval)
{
	Poco::Mutex::ScopedLock lock(mutex_);
	getMiningInfoInterval_ = interval;
}

void Burst::MinerConfig::setBufferChunkCount(unsigned bufferChunkCount)
{
	Poco::Mutex::ScopedLock lock(mutex_);
	bufferChunkCount_ = bufferChunkCount;
}

void Burst::MinerConfig::setPoolTargetDeadline(Poco::UInt64 targetDeadline)
{
	Poco::Mutex::ScopedLock lock(mutex_);
	targetDeadlinePool_ = targetDeadline;
}

void Burst::MinerConfig::setProcessorType(const std::string& processorType)
{
	Poco::Mutex::ScopedLock lock(mutex_);
	processorType_ = processorType;
}

void Burst::MinerConfig::setCpuInstructionSet(const std::string& instructionSet)
{
	Poco::Mutex::ScopedLock lock(mutex_);
	cpuInstructionSet_ = instructionSet;
}

void Burst::MinerConfig::setGpuPlatform(const unsigned platformIndex)
{
	Poco::Mutex::ScopedLock lock(mutex_);
	gpuPlatform_ = platformIndex;
}

void Burst::MinerConfig::setGpuDevice(const unsigned deviceIndex)
{
	Poco::Mutex::ScopedLock lock(mutex_);
	gpuDevice_ = deviceIndex;
}

void Burst::MinerConfig::setPlotDirs(const std::vector<std::string>& plotDirs)
{
	Poco::Mutex::ScopedLock lock(mutex_);
	plotDirs_.clear();
	for (const auto& plotDir : plotDirs)
		addPlotDir(plotDir);
}

void Burst::MinerConfig::setWebserverUri(const std::string& uri)
{
	Poco::Mutex::ScopedLock lock(mutex_);
	urlServer_ = uri;
}

void Burst::MinerConfig::setProgressbar(bool fancy, bool steady)
{
	Poco::Mutex::ScopedLock lock(mutex_);
	fancyProgressBar_ = fancy;
	steadyProgressBar_ = steady;
}

void Burst::MinerConfig::setPassphrase(const std::string& passphrase)
{
	Poco::Mutex::ScopedLock lock(mutex_);
	passphrase_.decrypted = passphrase;
	passphrase_.encrypt();
}

void Burst::MinerConfig::setWebserverCredentials(const std::string& user, const std::string& pass)
{
	Poco::Mutex::ScopedLock lock(mutex_);
	serverUser_.decrypted = user;
	serverPass_.decrypted = pass;

	serverUser_.encrypt();
	serverPass_.encrypt();
}

void Burst::MinerConfig::setStartWebserver(bool start)
{
	Poco::Mutex::ScopedLock lock(mutex_);
	startServer_ = start;
}

void Burst::MinerConfig::setDatabasePath(std::string databasePath)
{
	databasePath_ = std::move(databasePath);
}

bool Burst::MinerConfig::addPlotDir(const std::string& dir)
{
	return addPlotDir(std::make_shared<PlotDir>(Poco::replace(dir, "\\", "/"), PlotDir::Type::Sequential));
}

bool Burst::MinerConfig::removePlotDir(const std::string& dir)
{
	Poco::Mutex::ScopedLock lock(mutex_);

	const auto normalizedPath = Poco::replace(dir, "\\", "/");

	const auto iter = std::find_if(plotDirs_.begin(), plotDirs_.end(), [&](const std::shared_ptr<PlotDir>& element)
	{
		// TODO: look in Burst::MinerConfig::addPlotDir
		return element->getPath() == normalizedPath;
	});

	if (iter == plotDirs_.end())
		throw Poco::Exception{Poco::format("The plot dir '%s' does not exist", dir)};

	plotDirs_.erase(iter);
	return true;
}

std::string Burst::Passphrase::toString() const
{
	std::stringstream sstream;
	
	if (isOneWayHash())
		sstream <<
			delimiter << salt <<
			delimiter << encrypted;
	else
		sstream <<
			delimiter << algorithm <<
			delimiter << iterations <<
			delimiter << salt <<
			delimiter << key <<
			delimiter << encrypted;

	return sstream.str();	
}

bool Burst::Passphrase::isOneWayHash() const
{
	return algorithm.empty() && key.empty();
}

bool Burst::Passphrase::isTwoWayHash() const
{
	return !isOneWayHash();
}

bool Burst::Passphrase::isEncrypted() const
{
	return !encrypted.empty();
}

bool Burst::Passphrase::isPlainText() const
{
	return !decrypted.empty();
}

bool Burst::Passphrase::check(const std::string& plainText) const
{
	// if there is no hash
	if (encrypted.empty())
		// there is no password
		return plainText.empty();

	Passphrase toCheck;
	toCheck.algorithm = algorithm;
	toCheck.iterations = iterations;
	toCheck.salt = salt;
	toCheck.decrypted = plainText;
	return toCheck.encrypt() == encrypted;
}

bool Burst::Passphrase::empty() const
{
	return decrypted.empty() && encrypted.empty();
}

Burst::Passphrase Burst::Passphrase::fromString(const std::string& string)
{
	std::smatch match;
	Passphrase passphrase{};

	if (std::regex_match(string, match, std::regex{"::::(.*)::::(.*)::::(.*)::::(.*)::::(.*)"}))
	{
		passphrase.algorithm = match[1].str();
		passphrase.iterations = Poco::NumberParser::parse(match[2].str());
		passphrase.salt = match[3].str();
		passphrase.key = match[4].str();
		passphrase.encrypted = match[5].str();
	}
	else if (std::regex_match(string, match, std::regex{"::::(.*)::::(.*)"}))
	{
		passphrase.salt = match[1].str();
		passphrase.encrypted = match[2].str();
	}
	else if (std::regex_match(string, match, std::regex{"::::(.*)"}))
	{
		passphrase.encrypted = match[1].str();
	}
	else
	{
		passphrase.decrypted = string;
	}

	return passphrase;
}

std::string Burst::Passphrase::createSalt(size_t length)
{
	std::stringstream sstream;
	Poco::Random random;
	random.seed();

	for (size_t i = 0; i < length; ++i)
		sstream << random.nextChar();

	return toHex(sstream.str());
}

const std::string& Burst::Passphrase::decrypt()
{
	if (isTwoWayHash())
		decrypted = Burst::decrypt(encrypted, algorithm, key, salt, iterations);

	return decrypted;
}

const std::string& Burst::Passphrase::encrypt()
{
	if (salt.empty())
		salt = createSalt();

	if (isOneWayHash())
		encrypted = hashHmacSha1(decrypted, salt);

	if (isTwoWayHash())
	{
		if (algorithm.empty())
			algorithm = "aes-256-cbc";

		if (key.empty())
			key = createSalt();

		if (iterations == 0)
			iterations = 1000;
		
		const Poco::Crypto::CipherKey cipherKey(algorithm, key, salt, iterations);
		auto& factory = Poco::Crypto::CipherFactory::defaultFactory();
		auto cipher = factory.createCipher(cipherKey);
		
		encrypted = cipher->encryptString(decrypted, Poco::Crypto::Cipher::ENC_BINHEX_NO_LF);
	}

	decrypted.clear();

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

unsigned Burst::MinerConfig::getMiningInfoInterval() const
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

unsigned Burst::MinerConfig::getBufferChunkCount() const
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

bool Burst::MinerConfig::isForwardingMinerName() const
{
	return minerNameForwarding_;
}

Poco::UInt64 Burst::MinerConfig::getPoc2StartBlock() const
{
	return poc2StartBlock_;
}

bool Burst::MinerConfig::isVerboseLogging() const
{
	return verboseLogging_;
}
