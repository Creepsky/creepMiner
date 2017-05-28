//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#include "MinerUtil.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include "Declarations.hpp"
#include "MinerLogger.hpp"
#include <Poco/URI.h>
#include <Poco/Net/HTTPClientSession.h>
#include "Deadline.hpp"
#include <Poco/JSON/Object.h>
#include "MinerConfig.hpp"
#include "MinerData.hpp"
#include "PlotReader.hpp"
#include <Poco/JSON/Parser.h>
#include <locale>
#include <regex>
#include <Poco/FileStream.h>
#include <Poco/Crypto/CipherKey.h>
#include <Poco/Crypto/Cipher.h>
#include <Poco/Crypto/CipherFactory.h>
#include <Poco/Random.h>
#include <Poco/NestedDiagnosticContext.h>
#include "Account.hpp"
#include <Poco/HMACEngine.h>
#include <Poco/SHA1Engine.h>
#include <Poco/File.h>
#include <fstream>

bool Burst::isNumberStr(const std::string& str)
{
	return std::all_of(str.begin(), str.end(), ::isdigit);
}

std::string Burst::getFileNameFromPath(const std::string& strPath)
{
	size_t iLastSeparator;
	return strPath.substr((iLastSeparator = strPath.find_last_of("/\\")) !=
						  std::string::npos ? iLastSeparator + 1 : 0, strPath.size() - strPath.find_last_of("."));
}

std::vector<std::string> Burst::splitStr(const std::string& s, char delim)
{
	std::vector<std::string> elems;
	splitStr(s, delim, elems);
	return elems;
}

std::vector<std::string> Burst::splitStr(const std::string& s, const std::string& delim)
{
	std::vector<std::string> tokens;
	std::string::size_type pos, lastPos = 0, length = s.length();

	using size_type  = std::vector<std::string>::size_type;

	while(lastPos < length + 1)
	{
		pos = s.find_first_of(delim, lastPos);

		if(pos == std::string::npos)
			pos = length;

		if(pos != lastPos)
			tokens.push_back(std::string(s.data() + lastPos,
			static_cast<size_type>(pos-lastPos)));

		lastPos = pos + 1;
	}

	return tokens;
}

std::vector<std::string>& Burst::splitStr(const std::string& s, char delim, std::vector<std::string>& elems)
{
	std::stringstream ss(s);
	std::string item;

	while (std::getline(ss, item, delim))
		elems.push_back(item);

	return elems;
}

Burst::PlotCheckResult Burst::isValidPlotFile(const std::string& filePath)
{
	try
	{
		auto fileName = getFileNameFromPath(filePath);

		auto accountIdStr = getAccountIdFromPlotFile(fileName);
		auto nonceStartStr = getStartNonceFromPlotFile(fileName);
		auto nonceCountStr = getNonceCountFromPlotFile(fileName);
		auto staggerStr = getStaggerSizeFromPlotFile(fileName);

		if (accountIdStr == "" ||
			nonceStartStr == "" ||
			nonceCountStr == "" ||
			staggerStr == "")
			return PlotCheckResult::EmptyParameter;

		volatile auto accountId = std::stoull(accountIdStr);
		volatile auto nonceStart = std::stoull(nonceStartStr);
		volatile auto nonceCount = std::stoull(nonceCountStr);
		volatile auto staggerSize = std::stoull(staggerStr);

		// values are 0
		if (accountId == 0 ||
			nonceCount == 0 ||
			staggerSize == 0)
			return PlotCheckResult::InvalidParameter;

		// stagger not multiplier of nonce count
		if (nonceCount % staggerSize != 0)
			return PlotCheckResult::WrongStaggersize;

		Poco::File file{ filePath };
		
		// file is incomplete
		if (nonceCount * Settings::PlotSize != file.getSize())
			return PlotCheckResult::Incomplete;

		std::ifstream alternativeFileData{ filePath + ":stream" };

		if (alternativeFileData)
		{
			std::string content(std::istreambuf_iterator<char>(alternativeFileData), {});
			alternativeFileData.close();

			auto noncesWrote = reinterpret_cast<const uint64_t*>(content.data());

			if (*noncesWrote != nonceCount)
				return PlotCheckResult::Incomplete;
		}

		return PlotCheckResult::Ok;
	}
	catch (...)
	{
		return PlotCheckResult::Error;
	}
}

std::string Burst::getAccountIdFromPlotFile(const std::string& path)
{
	return getInformationFromPlotFile(path, 0);
}

std::string Burst::getNonceCountFromPlotFile(const std::string& path)
{
	return getInformationFromPlotFile(path, 2);
}

std::string Burst::getStaggerSizeFromPlotFile(const std::string& path)
{
	return getInformationFromPlotFile(path, 3);
}

std::string Burst::getStartNonceFromPlotFile(const std::string& path)
{
	auto filenamePos = path.find_last_of("/\\");

	if (filenamePos == std::string::npos)
		filenamePos = 0;

	auto fileNamePart = splitStr(path.substr(filenamePos + 1, path.length() - (filenamePos + 1)), '_');

	if (fileNamePart.size() > 3)
	{
		auto nonceStartPart = fileNamePart[1];

		if (isNumberStr(nonceStartPart))
			return nonceStartPart;
	}

	return "";
}

std::string Burst::deadlineFormat(uint64_t seconds)
{
	auto secs = seconds;
	auto mins = secs / 60;
	auto hours = mins / 60;
	auto day = hours / 24;
	auto months = day / 30;
	auto years = months / 12;
	
	std::stringstream ss;
	
	ss.imbue(std::locale(""));
	ss << std::fixed;
	
	if (years > 0)
		ss << years << "y ";
	if (months > 0)
		ss << months % 12 << "m ";
	if (day > 0)
		ss << day % 30 << "d ";

	ss << std::setw(2) << std::setfill('0');
	ss << hours % 24 << ':';
	ss << std::setw(2) << std::setfill('0');
	ss << mins % 60 << ':';
	ss << std::setw(2) << std::setfill('0');
	ss << secs % 60;

	return ss.str();
}

uint64_t Burst::deadlineFragment(uint64_t seconds, DeadlineFragment fragment)
{
	auto secs = seconds;
	auto mins = secs / 60;
	auto hours = mins / 60;
	auto day = hours / 24;
	auto months = day / 30;
	auto years = months / 12;

	switch (fragment)
	{
	case DeadlineFragment::Years: return years;
	case DeadlineFragment::Months: return months % 12;
	case DeadlineFragment::Days: return day % 30;
	case DeadlineFragment::Hours: return hours % 24;
	case DeadlineFragment::Minutes: return mins % 60;
	case DeadlineFragment::Seconds: return secs % 60;
	default: return 0;
	}
}

uint64_t Burst::formatDeadline(const std::string& format)
{
	if (format.empty())
		return 0;

	auto tokens = splitStr(format, ' ');

	if (tokens.empty())
		return 0;

	auto deadline = 0u;
	std::locale loc;

	std::regex years("\\d*y");
	std::regex months("\\d*m");
	std::regex days("\\d*d");
	std::regex hms("\\d\\d:\\d\\d:\\d\\d");

	const auto extractFunction = [](const std::string& token, uint32_t conversion, uint32_t postfixSize = 1)
	{
		return stoul(token.substr(0, token.size() - postfixSize)) * conversion;
	};

	for (auto& token : tokens)
	{
		if (regex_match(token, years))
			deadline += extractFunction(token, 60 * 60 * 24 * 30 * 12);
		else if (regex_match(token, months))
			deadline += extractFunction(token, 60 * 60 * 24 * 30);
		else if (regex_match(token, days))
			deadline += extractFunction(token, 60 * 60 * 24);
		else if (regex_match(token, hms))
		{
			auto subTokens = splitStr(token, ':');

			deadline += extractFunction(subTokens[0], 60 * 60, 0);
			deadline += extractFunction(subTokens[1], 60, 0);
			deadline += extractFunction(subTokens[2], 1, 0);
		}
	}

	return deadline;
}

std::string Burst::gbToString(uint64_t size)
{
	return memToString(size, MemoryUnit::Gigabyte, 2);
}

std::string Burst::memToString(uint64_t size, MemoryUnit factor, uint8_t precision)
{	
	std::stringstream ss;
	ss << std::fixed << std::setprecision(precision);
	ss << static_cast<double>(size) / static_cast<uint64_t>(factor);
	return ss.str();
}

std::string Burst::memToString(uint64_t size, uint8_t precision)
{
	if (size >= static_cast<uint64_t>(MemoryUnit::Exabyte))
		return memToString(size, MemoryUnit::Exabyte, precision) + " EB";
	else if (size >= static_cast<uint64_t>(MemoryUnit::Petabyte))
		return memToString(size, MemoryUnit::Petabyte, precision) + " PB";
	else if (size >= static_cast<uint64_t>(MemoryUnit::Terabyte))
		return memToString(size, MemoryUnit::Terabyte, precision) + " TB";
	else if (size >= static_cast<uint64_t>(MemoryUnit::Gigabyte))
		return memToString(size, MemoryUnit::Gigabyte, precision) + " GB";
	else
		return memToString(size, MemoryUnit::Megabyte, precision) + " MB";
}

std::string Burst::getInformationFromPlotFile(const std::string& path, uint8_t index)
{
	auto filenamePos = path.find_last_of("/\\");

	if (filenamePos == std::string::npos)
		filenamePos = 0;

	auto fileNamePart = splitStr(path.substr(filenamePos + 1, path.length() - (filenamePos + 1)), '_');

	if (index >= fileNamePart.size())
		return "";

	return fileNamePart[index];
}

std::string Burst::encrypt(const std::string& decrypted, const std::string& algorithm, std::string& key, std::string& salt, uint32_t& iterations)
{
	poco_ndc(encryptAES256);
	
	if (decrypted.empty())
		return "";
	
	if (iterations == 0)
		iterations = 1000;

	try
	{
		// all valid chars for the salt
		std::string validChars = "abcdefghijklmnopqrstuvwxyz";
		validChars += Poco::toUpper(validChars);
		validChars += "0123456789";
		validChars += "()[]*/+-#'~?�`&$!";

		const auto createRandomCharSequence = [&validChars](size_t lenght)
		{
			std::stringstream stream;
			Poco::Random random;

			random.seed();

			for (auto i = 0u; i < lenght; ++i)
				stream << validChars[random.next(static_cast<uint32_t>(validChars.size()))];

			return stream.str();
		};

		// we create a 30 chars long key if the param key is empty
		if (key.empty())
			key = createRandomCharSequence(30);

		// we create a 15 chars long salt if the param salt is empty
		if (salt.empty())
			salt = createRandomCharSequence(15);

		Poco::Crypto::CipherKey cipherKey(algorithm, key, salt, iterations);
		auto& factory = Poco::Crypto::CipherFactory::defaultFactory();
		auto cipher = factory.createCipher(cipherKey);
		
		return cipher->encryptString(decrypted, Poco::Crypto::Cipher::ENC_BASE64);
	}
	catch (Poco::Exception& exc)
	{
		log_error(MinerLogger::general, "Error encrypting the passphrase!\n%s", exc.displayText());
		log_current_stackframe(MinerLogger::general);

		return "";
	}
}

std::string Burst::decrypt(const std::string& encrypted, const std::string& algorithm, const std::string& key, const std::string& salt, uint32_t& iterations)
{
	poco_ndc(decryptAES256);

	if (iterations == 0)
		iterations = 1000;

	try
	{
		Poco::Crypto::CipherKey cipherKey(algorithm, key, salt, iterations);
		auto& factory = Poco::Crypto::CipherFactory::defaultFactory();
		auto cipher = factory.createCipher(cipherKey);
		return cipher->decryptString(encrypted, Poco::Crypto::Cipher::ENC_BASE64);
	}
	catch (Poco::Exception& exc)
	{
		log_error(MinerLogger::general, "Error decrypting the passphrase!\n%s", exc.displayText());
		log_current_stackframe(MinerLogger::general);

		return "";
	}
}

Poco::Timespan Burst::secondsToTimespan(float seconds)
{
	auto secondsInt = static_cast<long>(seconds);
	auto microSeconds = static_cast<long>((seconds - secondsInt) * 100000);
	return Poco::Timespan{secondsInt, microSeconds};
}

std::unique_ptr<Poco::Net::HTTPClientSession> Burst::createSession(const Poco::URI& uri)
{
	return nullptr;
}

Poco::Net::SocketAddress Burst::getHostAddress(const Poco::URI& uri)
{
	Poco::Net::SocketAddress address{uri.getHost() + ':' + std::to_string(uri.getPort())};
	return address;
}

std::string Burst::serializeDeadline(const Deadline& deadline, std::string delimiter)
{
	return deadline.getAccountName() + delimiter +
		std::to_string(deadline.getBlock()) + delimiter +
		std::to_string(deadline.getDeadline()) + delimiter +
		std::to_string(deadline.getNonce());
}

Poco::JSON::Object Burst::createJsonDeadline(const Deadline& deadline)
{
	Poco::JSON::Object json;
	json.set("nonce", std::to_string(deadline.getNonce()));
	json.set("deadline", deadlineFormat(deadline.getDeadline()));
	json.set("account", deadline.getAccountName());
	json.set("accountId", std::to_string(deadline.getAccountId()));
	json.set("plotfile", deadline.getPlotFile());
	json.set("deadlineNum", std::to_string(deadline.getDeadline()));
	return json;
}

Poco::JSON::Object Burst::createJsonDeadline(const Deadline& deadline, const std::string& type)
{
	auto json = createJsonDeadline(deadline);
	json.set("type", type);
	json.set("time", getTime());
	return json;
}

Poco::JSON::Object Burst::createJsonNewBlock(const MinerData& data)
{
	Poco::JSON::Object json;
	auto blockPtr = data.getBlockData();

	if (blockPtr == nullptr)
		return json;

	auto& block = *blockPtr;
	auto bestOverall = data.getBestDeadlineOverall();

	json.set("type", "new block");
	json.set("block", std::to_string(block.getBlockheight()));
	json.set("scoop", std::to_string(block.getScoop()));
	json.set("baseTarget", std::to_string(block.getBasetarget()));
	json.set("gensigStr", block.getGensigStr());
	json.set("time", getTime());
	json.set("blocksMined", std::to_string(data.getBlocksMined()));
	json.set("blocksWon", std::to_string(data.getBlocksWon()));
	if (bestOverall != nullptr)
	{
		json.set("bestOverallNum", std::to_string(bestOverall->getDeadline()));
		json.set("bestOverall", deadlineFormat(bestOverall->getDeadline()));
	}
	json.set("deadlinesConfirmed", std::to_string(data.getConfirmedDeadlines()));
	json.set("deadlinesAvg", deadlineFormat(data.getAverageDeadline()));
		
	Poco::JSON::Array bestDeadlines;

	for (auto& historicalDeadline : data.getAllHistoricalBlockData())
	{
		if (historicalDeadline->getBestDeadline() != nullptr)
		{
			Poco::JSON::Array jsonBestDeadline;
			jsonBestDeadline.add(std::to_string(historicalDeadline->getBlockheight()));
			jsonBestDeadline.add(std::to_string(historicalDeadline->getBestDeadline()->getDeadline()));
			bestDeadlines.add(jsonBestDeadline);
		}
	}

	json.set("bestDeadlines", bestDeadlines);

	return json;
}

Poco::JSON::Object Burst::createJsonConfig()
{
	Poco::JSON::Object json;
	auto targetDeadline = MinerConfig::getConfig().getTargetDeadline();

	json.set("type", "config");
	json.set("poolUrl", MinerConfig::getConfig().getPoolUrl().getCanonical(true));
	json.set("poolUrlPort", std::to_string(MinerConfig::getConfig().getPoolUrl().getPort()));
	json.set("miningInfoUrl", MinerConfig::getConfig().getMiningInfoUrl().getCanonical(true));
	json.set("miningInfoUrlPort", std::to_string(MinerConfig::getConfig().getMiningInfoUrl().getPort()));
	json.set("walletUrl", MinerConfig::getConfig().getWalletUrl().getCanonical(true));
	json.set("walletUrlPort", std::to_string(MinerConfig::getConfig().getWalletUrl().getPort()));
	json.set("totalPlotSize", memToString(MinerConfig::getConfig().getTotalPlotsize(), 2));
	json.set("timeout", MinerConfig::getConfig().getTimeout());
	json.set("bufferSize", memToString(MinerConfig::getConfig().getMaxBufferSize() * 1024 * 1024, 0));
	json.set("bufferSizeMB", std::to_string(MinerConfig::getConfig().getMaxBufferSize()));
	json.set("targetDeadline", deadlineFormat(targetDeadline));
	json.set("maxPlotReaders", std::to_string(MinerConfig::getConfig().getMaxPlotReaders()));
	json.set("miningIntensity", std::to_string(MinerConfig::getConfig().getMiningIntensity()));
	json.set("submissionMaxRetry", std::to_string(MinerConfig::getConfig().getSubmissionMaxRetry()));
	
	Poco::JSON::Object jsonTargerDeadline;
	jsonTargerDeadline.set("years", deadlineFragment(targetDeadline, DeadlineFragment::Years));
	jsonTargerDeadline.set("months", deadlineFragment(targetDeadline, DeadlineFragment::Months));
	jsonTargerDeadline.set("days", deadlineFragment(targetDeadline, DeadlineFragment::Days));
	jsonTargerDeadline.set("hours", deadlineFragment(targetDeadline, DeadlineFragment::Hours));
	jsonTargerDeadline.set("minutes", deadlineFragment(targetDeadline, DeadlineFragment::Minutes));
	jsonTargerDeadline.set("seconds", deadlineFragment(targetDeadline, DeadlineFragment::Seconds));

	json.set("targetDeadlineObject", jsonTargerDeadline);

	return json;
}

Poco::JSON::Object Burst::createJsonProgress(float progress)
{
	Poco::JSON::Object json;
	json.set("type", "progress");
	json.set("value", progress);
	return json;
}

Poco::JSON::Object Burst::createJsonLastWinner(const MinerData& data)
{
	auto block = data.getBlockData();

	if (block == nullptr || block->getLastWinner() == nullptr)
		return Poco::JSON::Object{};

	return *block->getLastWinner()->toJSON();
}

Poco::JSON::Object Burst::createJsonShutdown()
{
	Poco::JSON::Object json;
	json.set("shutdown", true);
	return json;
}

Poco::JSON::Object Burst::createJsonWonBlocks(const MinerData& data)
{
	Poco::JSON::Object json;
	json.set("type", "blocksWonUpdate");
	json.set("blocksWon", std::to_string(data.getBlocksWon()));
	return json;
}

Poco::JSON::Object Burst::createJsonPlotDir(const PlotDir& plotDir)
{
	Poco::JSON::Object json;

	json.set("path", plotDir.getPath());

	Poco::JSON::Array jsonPlotFiles;

	for (const auto& plotFile : plotDir.getPlotfiles())
	{
		Poco::JSON::Object jsonPlotFile;

		jsonPlotFile.set("path", plotFile->getPath());
		jsonPlotFile.set("size", memToString(plotFile->getSize(), 2));

		jsonPlotFiles.add(jsonPlotFile);
	}

	json.set("plotfiles", jsonPlotFiles);
	json.set("size", memToString(plotDir.getSize(), 2));

	return json;
}

Poco::JSON::Array Burst::createJsonPlotDirs()
{
	Poco::JSON::Array jsonPlotDirs;

	MinerConfig::getConfig().forPlotDirs([&jsonPlotDirs](PlotDir& plotDir)
	{
		jsonPlotDirs.add(createJsonPlotDir(plotDir));

		for (const auto& relatedPlotDir : plotDir.getRelatedDirs())
			jsonPlotDirs.add(createJsonPlotDir(*relatedPlotDir));

		return true;
	});

	return jsonPlotDirs;
}

Poco::JSON::Object Burst::createJsonPlotDirsRescan()
{
	Poco::JSON::Object jsonPlotRescan;
	jsonPlotRescan.set("type", "plotdirs-rescan");
	jsonPlotRescan.set("plotdirs", createJsonPlotDirs());
	return jsonPlotRescan;
}

std::string Burst::getTime()
{
	std::stringstream ss;

#if defined(__linux__) && __GNUC__ < 5
	time_t rawtime;
	struct tm * timeinfo;
	char buffer [80];

	time (&rawtime);
	timeinfo = localtime (&rawtime);

	strftime (buffer, 80, "%X",timeinfo);

	ss << buffer;
#else 
	auto now = std::chrono::system_clock::now();
	auto now_c = std::chrono::system_clock::to_time_t(now);
	ss.imbue(std::locale());
	ss << std::put_time(std::localtime(&now_c), "%X");
#endif

	return ss.str();
}

std::string Burst::getFilenameWithtimestamp(const std::string& name, const std::string& ending)
{
	return Poco::format("%s_%s.%s",
		name, Poco::DateTimeFormatter::format(Poco::Timestamp(), "%Y%m%d_%H%M%s"), ending);
}

std::string Burst::hash_HMAC_SHA1(const std::string& plain, const std::string& passphrase)
{
	Poco::HMACEngine<Poco::SHA1Engine> engine{ passphrase };
	engine.update(plain);
	auto& digest = engine.digest();
	return Poco::DigestEngine::digestToHex(digest);
}

bool Burst::check_HMAC_SHA1(const std::string& plain, const std::string& hashed, const std::string& passphrase)
{
	// if there is no hash
	if (hashed.empty())
		// there is no password
		return plain.empty();

	// first, hash the plain text
	Poco::HMACEngine<Poco::SHA1Engine> engine{ passphrase };
	//
	engine.update(plain);
	//
	auto& digest = engine.digest();

	// create the digest for the hashed word
	auto hashedDigest = Poco::HMACEngine<Poco::SHA1Engine>::digestFromHex(hashed);

	// check if its the same
	return digest == hashedDigest;
}
