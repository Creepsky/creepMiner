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
#include <Poco/SHA1Engine.h>
#include <Poco/DigestStream.h>
#include "PlotSizes.hpp"
#include <Poco/Logger.h>
#include <Poco/SplitterChannel.h>
#include "Output.hpp"

const std::string Burst::MinerConfig::WebserverPassphrase = "secret-webserver-pass-951";

void Burst::MinerConfig::rescan()
{
	readConfigFile(configPath_);
}

Burst::PlotFile::PlotFile(std::string&& path, uint64_t size)
	: path_(move(path)), size_(size)
{}

const std::string& Burst::PlotFile::getPath() const
{
	return path_;
}

uint64_t Burst::PlotFile::getSize() const
{
	return size_;
}

Burst::PlotDir::PlotDir(std::string plotPath, Type type)
	: path_{std::move(plotPath)},
	  type_{type},
	  size_{0}
{
	addPlotLocation(path_);
}

Burst::PlotDir::PlotDir(std::string path, const std::vector<std::string>& relatedPaths, Type type)
	: path_{std::move(path)},
	  type_{type},
	  size_{0}
{
	addPlotLocation(path_);

	for (const auto& relatedPath : relatedPaths)
		relatedDirs_.emplace_back(new PlotDir{ relatedPath, type_ });
}

const std::vector<std::shared_ptr<Burst::PlotFile>>& Burst::PlotDir::getPlotfiles() const
{
	return plotfiles_;
}

const std::string& Burst::PlotDir::getPath() const
{
	return path_;
}

uint64_t Burst::PlotDir::getSize() const
{
	return size_;
}

Burst::PlotDir::Type Burst::PlotDir::getType() const
{
	return type_;
}

std::vector<std::shared_ptr<Burst::PlotDir>> Burst::PlotDir::getRelatedDirs() const
{
	return relatedDirs_;
}

bool Burst::PlotDir::addPlotLocation(const std::string& fileOrPath)
{
	try
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
			return addPlotFile(fileOrPath) != nullptr;

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
	catch (...)
	{
		return false;
	}
}

std::shared_ptr<Burst::PlotFile> Burst::PlotDir::addPlotFile(const Poco::File& file)
{
	auto result = isValidPlotFile(file.path());

	if (result == PlotCheckResult::Ok)
	{
		// plot file is already in our list
		for (size_t i = 0; i < plotfiles_.size(); i++)
			if (plotfiles_[i]->getPath() == file.path())
				return plotfiles_[i];

		// make a new plotfile and add it to the list
		auto plotFile = std::make_shared<PlotFile>(std::string(file.path()), file.getSize());
		plotfiles_.emplace_back(plotFile);
		size_ += file.getSize();

		return plotFile;
	}

	std::string errorString = "";

	if (result == PlotCheckResult::Incomplete)
		errorString = "The plotfile is incomplete!";

	if (result == PlotCheckResult::EmptyParameter)
		errorString = "The plotfile does not have all the required parameters!";

	if (result == PlotCheckResult::InvalidParameter)
		errorString = "The plotfile has invalid parameters!";

	if (result == PlotCheckResult::WrongStaggersize)
		errorString = "The plotfile has an invalid staggersize!";

	log_warning(MinerLogger::config, "Found an invalid plotfile, skipping it!\n\tPath: %s\n\tReason: %s", file.path(), errorString);
	return nullptr;
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

template <typename T>
T getOrAddAlt(Poco::JSON::Object::Ptr object, Poco::JSON::Object::Ptr alt, const std::string& key, T defaultValue, std::string altkey = "")
{
	auto json = object->get(key);
	Poco::Dynamic::Var jsonAlt;

	if (altkey.empty())
		altkey = key;

	if (!alt.isNull() && alt->has(altkey))
		jsonAlt = alt->get(altkey);

	alt->remove(altkey);

	T defaultOrAltValue = defaultValue;

	if (!jsonAlt.isEmpty())
		defaultOrAltValue = jsonAlt.extract<T>();

	if (json.isEmpty())
		object->set(key, defaultOrAltValue);
	else if (json.type() == typeid(T))
		return json.extract<T>();

	return defaultOrAltValue;
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
		const std::string& defaultScheme, unsigned short defaultPort, const std::string& createUrl, std::string altName = "", bool forceInsert = false)
	{
		if (altName.empty())
			altName = name;

		auto var = getOrAddAlt(urlsObj, config, name, createUrl, altName);

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
			try
			{
				auto logPathObj = loggingObj->get("path");
				std::string logPath = "";

				if (logPathObj.isEmpty())
					loggingObj->set("path", "");
				else if (logPathObj.isString())
					logPath = logPathObj.extract<std::string>();

				auto logDir = MinerLogger::setLogDir(logPath);
				pathLogfile_ = logDir;

				if (!logPath.empty())
					log_system(MinerLogger::config, "Changing path for log file to\n\t%s", logDir);
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

		// output
		{
			Poco::JSON::Object::Ptr outputObj = nullptr;

			// the output element can occur in two places:
			//  a) the old one, directly under the root element
			//  b) the new one, inside the logging element
			auto outputLoggingObj = readOutput(loggingObj->getObject("output"));
			auto outputConfigObj = readOutput(config->getObject("output"));

			// the plan is
			//  - use the new one
			//  - if its not there, but the old one exists, move it to the new place
			//  - otherwise create it

			// new one exists
			if (!outputLoggingObj.isNull())
				outputObj = outputLoggingObj;
			// old one exists
			else if (!outputConfigObj.isNull())
			{
				// if only the old one exists, move it to the new place
				if (outputLoggingObj.isNull())
					outputObj = outputConfigObj;

				// delete the old one
				config->remove("output");
			}
			// none exists
			else
				outputObj = new Poco::JSON::Object;

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

		submission_max_retry_ = getOrAddAlt(miningObj, config, "submissionMaxRetry", 3);
		maxBufferSizeMB = getOrAddAlt(miningObj, config, "maxBufferSizeMB", 128);

		if (maxBufferSizeMB == 0)
			maxBufferSizeMB = 128;

		auto timeout = getOrAddAlt(miningObj, config, "timeout", 30);
		timeout_ = static_cast<float>(timeout);

		miningIntensity_ = getOrAddAlt(miningObj, config, "intensity", 2, "miningIntensity");
		
		auto maxPlotReaders = getOrAddAlt(miningObj, config, "maxPlotReaders", 0);
		maxPlotReaders_ = maxPlotReaders;

		// urls
		{
			Poco::JSON::Object::Ptr urlsObj;

			if (miningObj->has("urls"))
				urlsObj = miningObj->get("urls").extract<Poco::JSON::Object::Ptr>();
			else
				urlsObj = new Poco::JSON::Object;

			checkCreateUrlFunc(urlsObj, "submission", urlPool_, "http", 8124, "http://pool.burst-team.us:8124", "poolUrl");
			checkCreateUrlFunc(urlsObj, "miningInfo", urlMiningInfo_, "http", 8124, "", "miningInfoUrl");
			checkCreateUrlFunc(urlsObj, "wallet", urlWallet_, "https", 8125, "https://wallet.burst-team.us:8128", "walletUrl");

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
				auto plotsArr = getOrAddAlt(miningObj, config, "plots", arr);

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
		}

		// target deadline
		{
			auto targetDeadline = miningObj->get("targetDeadline");
			auto targetDeadlineAlt = config->get("targetDeadline");

			if (targetDeadline.isEmpty())
				targetDeadline = targetDeadlineAlt;

			config->remove("targetDeadline");
			miningObj->set("targetDeadline", targetDeadline);

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
				miningObj->set("targetDeadline", "0y 0m 0d 00:00:00");
				targetDeadline_ = 0;
			}
		}

		// passphrase
		{
			try
			{
				auto passphraseJson = miningObj->get("passphrase");
				auto passphraseJsonAlt = config->get("passphrase");
				Poco::JSON::Object::Ptr passphrase = nullptr;

				if (passphraseJson.isEmpty())
					passphraseJson = passphraseJsonAlt;

				config->remove("passphrase");
				miningObj->set("passphrase", passphraseJson);

				if (!passphraseJson.isEmpty())
					passphrase = passphraseJson.extract<Poco::JSON::Object::Ptr>();

				if (passphrase.isNull())
				{
					passphrase = new Poco::JSON::Object;
					miningObj->set("passphrase", passphrase);
				}

				log_debug(MinerLogger::config, "Reading passphrase...");

				auto decrypted = getOrAdd<std::string>(passphrase, "decrypted", "");
				auto encrypted = getOrAdd<std::string>(passphrase, "encrypted", "");
				auto salt = getOrAdd<std::string>(passphrase, "salt", "");
				auto key = getOrAdd<std::string>(passphrase, "key", "");
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
						passphrase->set("encrypted", encrypted);
						passphrase->set("salt", salt);
						passphrase->set("key", key);
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

		startServer_ = getOrAddAlt(webserverObj, config, "start", true, "Start Server");
		checkCreateUrlFunc(webserverObj, "url", serverUrl_, "http", 8080, "http://localhost:8080", "serverUrl", startServer_);

		// credentials
		{
			auto credentialsJson = webserverObj->get("credentials");
			Poco::JSON::Object::Ptr credentials;

			const auto plainUserId = "plain-user";
			const auto plainPassId = "plain-pass";
			const auto hashedUserId = "hashed-user";
			const auto hashedPassId = "hashed-pass";
			
			if (!credentialsJson.isEmpty())
				credentials = credentialsJson.extract<Poco::JSON::Object::Ptr>();

			if (credentials.isNull())
			{
				credentials = new Poco::JSON::Object;
				webserverObj->set("credentials", credentials);
			}

			auto plainUser = getOrAdd(credentials, plainUserId, std::string{});
			auto hashedUser = getOrAdd(credentials, hashedUserId, std::string{});
			auto plainPass = getOrAdd(credentials, plainPassId, std::string{});
			auto hashedPass = getOrAdd(credentials, hashedPassId, std::string{});

			if (!plainUser.empty())
			{
				hashedUser = hash_HMAC_SHA1(plainUser, WebserverPassphrase);
				credentials->set(hashedUserId, hashedUser);
				credentials->set(plainUserId, "");

				log_debug(MinerLogger::config, "hashed  the webserver username\n"
					"\thash: %s", hashedUser);
			}

			if (!plainPass.empty())
			{
				hashedPass = hash_HMAC_SHA1(plainPass, WebserverPassphrase);
				credentials->set(hashedPassId, hashedPass);
				credentials->set(plainPassId, "");

				log_debug(MinerLogger::config, "hashed  the webserver password\n"
					"\thash: %s", hashedPass);
			}

			if (!hashedUser.empty())
				serverUser_ = hashedUser;

			if (!hashedPass.empty())
				serverPass_ = hashedPass;
		}

		config->set("webserver", webserverObj);
	}
	
	std::ofstream outputFileStream{configPath};

	std::stringstream sstr;
	config->stringify(sstr, 4);
	auto outJsonStr = sstr.str();

	Poco::replaceInPlace(outJsonStr, "\\/", "/");

	if (outputFileStream.is_open())
	{
		outputFileStream << outJsonStr << std::flush;
		outputFileStream.close();
	}

	return true;
}

const std::string& Burst::MinerConfig::getPath() const
{
	return configPath_;
}

std::vector<std::shared_ptr<Burst::PlotFile>> Burst::MinerConfig::getPlotFiles() const
{
	std::vector<std::shared_ptr<Burst::PlotFile>> plotFiles;

	for (auto& plotDir : plotDirs_)
	{
		plotFiles.insert(plotFiles.end(),
			plotDir->getPlotfiles().begin(), plotDir->getPlotfiles().end());
		
		for (const auto& relatedDir : plotDir->getRelatedDirs())
			plotFiles.insert(plotFiles.end(),
				relatedDir->getPlotfiles().begin(), relatedDir->getPlotfiles().end());
	}

	return plotFiles;
}

uintmax_t Burst::MinerConfig::getTotalPlotsize() const
{
	uint64_t sum = 0;

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

Poco::JSON::Object::Ptr Burst::MinerConfig::readOutput(Poco::JSON::Object::Ptr json)
{
	if (json.isNull())
		return nullptr;

	auto checkCreateFunc = [&json](int id, const std::string& name)
	{
		auto obj = json->get(name);

		if (!obj.isEmpty() && obj.isBoolean())
			MinerLogger::setOutput(id, obj.extract<bool>());
		else
			json->set(name, MinerLogger::hasOutput(id));
	};

	checkCreateFunc(LastWinner, "lastWinner");
	checkCreateFunc(NonceFound, "nonceFound");
	checkCreateFunc(NonceOnTheWay, "nonceOnTheWay");
	checkCreateFunc(NonceSent, "nonceSent");
	checkCreateFunc(NonceConfirmed, "nonceConfirmed");
	checkCreateFunc(PlotDone, "plotDone");
	checkCreateFunc(DirDone, "dirDone");

	return json;
}

uint32_t Burst::MinerConfig::getMiningIntensity() const
{
	return miningIntensity_;
}

const std::vector<std::shared_ptr<Burst::PlotDir>>& Burst::MinerConfig::getPlotDirs() const
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
	if (maxPlotReaders_ == 0)
		return static_cast<uint32_t>(plotDirs_.size());

	return maxPlotReaders_;
}

const Poco::Path& Burst::MinerConfig::getPathLogfile() const
{
	return pathLogfile_;
}

const std::string& Burst::MinerConfig::getServerUser() const
{
	return serverUser_;
}

const std::string& Burst::MinerConfig::getServerPass() const
{
	return serverPass_;
}
