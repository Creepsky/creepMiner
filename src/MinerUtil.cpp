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

#include "MinerUtil.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include "Declarations.hpp"
#include "logging/MinerLogger.hpp"
#include <Poco/URI.h>
#include <Poco/Net/HTTPClientSession.h>
#include "mining/Deadline.hpp"
#include <Poco/JSON/Object.h>
#include "mining/MinerConfig.hpp"
#include "mining/MinerData.hpp"
#include "plots/PlotReader.hpp"
#include <Poco/JSON/Parser.h>
#include <locale>
#include <regex>
#include <Poco/FileStream.h>
#include <Poco/Crypto/CipherKey.h>
#include <Poco/Crypto/Cipher.h>
#include <Poco/Crypto/CipherFactory.h>
#include <Poco/Random.h>
#include <Poco/NestedDiagnosticContext.h>
#include "wallet/Account.hpp"
#include <Poco/HMACEngine.h>
#include <Poco/SHA1Engine.h>
#include <Poco/File.h>
#include <fstream>
#include "plots/PlotSizes.hpp"
#include <chrono>
#include <Poco/StreamCopier.h>
#include <Poco/Base64Decoder.h>
#include <Poco/HexBinaryEncoder.h>
#include <Poco/HexBinaryDecoder.h>

#if defined(_WIN32)
#include <Windows.h>
#include <conio.h>
#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <termios.h>
#if defined(BSD)
#include <sys/sysctl.h>
#endif

#endif

// cpuinfo stuff (sse2, sse4, ...)
#ifdef _WIN32
//  Windows
#define cpuid(info, x) __cpuidex(info, x, 0)
#else
#ifdef __arm__
void cpuid(int info[4], int InfoType)
{}
#else
//  GCC Intrinsics
#include <cpuid.h>
void cpuid(int info[4], int InfoType)
{
	__cpuid_count(InfoType, 0, info[0], info[1], info[2], info[3]);
}
#endif
#endif

bool Burst::isNumberStr(const std::string& str)
{
	return std::all_of(str.begin(), str.end(), ::isdigit);
}

std::string Burst::getFileNameFromPath(const std::string& strPath)
{
	size_t iLastSeparator;
	return strPath.substr((iLastSeparator = strPath.find_last_of("/\\")) !=
						  std::string::npos ? iLastSeparator + 1 : 0, strPath.size() - strPath.find_last_of('.'));
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
	std::string::size_type lastPos = 0;
	const auto length = s.length();

	using SizeType  = std::vector<std::string>::size_type;

	while(lastPos < length + 1)
	{
		auto pos = s.find_first_of(delim, lastPos);

		if(pos == std::string::npos)
			pos = length;

		if(pos != lastPos)
			tokens.emplace_back(s.data() + lastPos,
			static_cast<SizeType>(pos-lastPos));

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
		const auto fileName = getFileNameFromPath(filePath);

		const auto accountIdStr = getAccountIdFromPlotFile(fileName);
		const auto nonceStartStr = getStartNonceFromPlotFile(fileName);
		const auto nonceCountStr = getNonceCountFromPlotFile(fileName);
		const auto staggerStr = getStaggerSizeFromPlotFile(fileName);

		if (accountIdStr.empty() ||
			nonceStartStr.empty() ||
			nonceCountStr.empty() ||
			staggerStr.empty())
			return PlotCheckResult::EmptyParameter;

		const volatile auto accountId = std::stoull(accountIdStr);
		std::stoull(nonceStartStr);
		const volatile auto nonceCount = std::stoull(nonceCountStr);
		const volatile auto staggerSize = std::stoull(staggerStr);

		// values are 0
		if (accountId == 0 ||
			nonceCount == 0 ||
			staggerSize == 0)
			return PlotCheckResult::InvalidParameter;

		// only do these checks if the user dont want to use insecure plotfiles (should be default)
		if (!MinerConfig::getConfig().useInsecurePlotfiles())
		{
			// stagger not multiplier of nonce count
			if (nonceCount % staggerSize != 0)
				return PlotCheckResult::WrongStaggersize;

			Poco::File file{ filePath };
		
			// file is incomplete
			if (nonceCount * Settings::plotSize != file.getSize())
				return PlotCheckResult::Incomplete;

			std::ifstream alternativeFileData{ filePath + ":stream" };

			if (alternativeFileData)
			{
				std::string content(std::istreambuf_iterator<char>(alternativeFileData), {});
				alternativeFileData.close();

				const auto noncesWrote = reinterpret_cast<const Poco::UInt64*>(content.data());

				if (*noncesWrote != nonceCount)
					return PlotCheckResult::Incomplete;
			}
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

std::string Burst::deadlineFormat(Poco::UInt64 seconds)
{
	const auto secs = seconds;
	const auto mins = secs / 60;
	const auto hours = mins / 60;
	const auto day = hours / 24;
	const auto months = day / 30;
	const auto years = months / 12;
	
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

Poco::UInt64 Burst::deadlineFragment(Poco::UInt64 seconds, Burst::DeadlineFragment fragment)
{
	const auto secs = seconds;
	const auto mins = secs / 60;
	const auto hours = mins / 60;
	const auto day = hours / 24;
	const auto months = day / 30;
	const auto years = months / 12;

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

Poco::UInt64 Burst::formatDeadline(const std::string& format)
{
	if (format.empty())
		return 0;

	auto tokens = splitStr(format, ' ');

	if (tokens.empty())
		return 0;

	Poco::UInt64 deadline = 0u;
	std::locale loc;

	const std::regex years("\\d*y");
	const std::regex months("\\d*m");
	const std::regex days("\\d*d");
	const std::regex hms(R"(\d\d:\d\d:\d\d)");

	const auto extractFunction = [](const std::string& token, uint32_t conversion, uint32_t postfixSize = 1)
	{
		return Poco::NumberParser::parseUnsigned64(token.substr(0, token.size() - postfixSize)) * conversion;
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

std::string Burst::gbToString(Poco::UInt64 size)
{
	return memToString(size, MemoryUnit::Gigabyte, 2);
}

std::string Burst::memToString(Poco::UInt64 size, MemoryUnit factor, Poco::UInt8 precision)
{	
	std::stringstream ss;
	ss << std::fixed << std::setprecision(precision);
	ss << static_cast<double>(size) / static_cast<Poco::UInt64>(factor);
	return ss.str();
}

std::string Burst::memToString(Poco::UInt64 size, Poco::UInt8 precision)
{
	if (size >= static_cast<Poco::UInt64>(MemoryUnit::Exabyte))
		return memToString(size, MemoryUnit::Exabyte, precision) + " EB";
	else if (size >= static_cast<Poco::UInt64>(MemoryUnit::Petabyte))
		return memToString(size, MemoryUnit::Petabyte, precision) + " PB";
	else if (size >= static_cast<Poco::UInt64>(MemoryUnit::Terabyte))
		return memToString(size, MemoryUnit::Terabyte, precision) + " TB";
	else if (size >= static_cast<Poco::UInt64>(MemoryUnit::Gigabyte))
		return memToString(size, MemoryUnit::Gigabyte, precision) + " GB";
	else
		return memToString(size, MemoryUnit::Megabyte, precision) + " MB";
}

std::string Burst::getInformationFromPlotFile(const std::string& path, Poco::UInt8 index)
{
	auto filenamePos = path.find_last_of("/\\");

	if (filenamePos == std::string::npos)
		filenamePos = 0;

	auto fileNamePart = splitStr(path.substr(filenamePos + 1, path.length() - (filenamePos + 1)), '_');

	if (index >= fileNamePart.size())
		return "";

	return fileNamePart[index];
}

std::string Burst::encrypt(const std::string& decrypted, const std::string& algorithm, std::string& key, std::string& salt, Poco::UInt32 iterations)
{
	poco_ndc(encryptAES256);
	
	if (decrypted.empty())
		return "";
	
	if (iterations == 0)
		return "";

	try
	{
		// all valid chars for the salt
		std::string validChars = "abcdefghijklmnopqrstuvwxyz";
		validChars += Poco::toUpper(validChars);
		validChars += "0123456789";
		validChars += "()[]*/+-#'~?&$!";

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

		const Poco::Crypto::CipherKey cipherKey(algorithm, key, salt, iterations);
		auto& factory = Poco::Crypto::CipherFactory::defaultFactory();
		auto cipher = factory.createCipher(cipherKey);
		
		return cipher->encryptString(decrypted, Poco::Crypto::Cipher::ENC_BINHEX_NO_LF);
	}
	catch (Poco::Exception& exc)
	{
		log_error(MinerLogger::general, "Error encrypting the passphrase!\n%s", exc.displayText());
		log_current_stackframe(MinerLogger::general);

		return "";
	}
}

std::string Burst::decrypt(const std::string& encrypted, const std::string& algorithm, const std::string& key, const std::string& salt, Poco::UInt32 iterations)
{
	poco_ndc(decryptAES256);

	if (iterations == 0)
		return "";

	try
	{
		const Poco::Crypto::CipherKey cipherKey(algorithm, key, salt, iterations);
		auto& factory = Poco::Crypto::CipherFactory::defaultFactory();
		auto cipher = factory.createCipher(cipherKey);
		return cipher->decryptString(encrypted, Poco::Crypto::Cipher::ENC_BINHEX_NO_LF);
	}
	catch (Poco::Exception& exc)
	{
		log_error(MinerLogger::general, "Error decrypting the passphrase!\n%s", exc.displayText());
		log_current_stackframe(MinerLogger::general);

		return "";
	}
}

Poco::Timespan Burst::secondsToTimespan(const float seconds)
{
	const auto secondsInt = static_cast<long>(seconds);
	const auto microSeconds = static_cast<long>((seconds - secondsInt) * 100000);
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

std::string Burst::serializeDeadline(const Deadline& deadline, const std::string& delimiter)
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
	json.set("blockheight", std::to_string(deadline.getBlock()));
	json.set("miner", deadline.getMiner());
	json.set("worker", deadline.getWorker());
	json.set("ip", deadline.getIp().toString());
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
	const auto blockPtr = data.getBlockData();

	if (blockPtr == nullptr)
		return json;

	auto& block = *blockPtr;
	const auto bestOverall = data.getBestDeadlineOverall();
	const auto bestHistorical = data.getBestDeadlineOverall(true);

	json.set("type", "new block");
	json.set("block", std::to_string(block.getBlockheight()));
	json.set("scoop", std::to_string(block.getScoop()));
	json.set("targetDeadline", std::to_string(block.getBlockTargetDeadline()));
	json.set("baseTarget", std::to_string(block.getBasetarget()));
	json.set("gensigStr", block.getGensigStr());
	json.set("time", getTime());
	json.set("startTime", std::to_string( std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count() ));
	json.set("blocksMined", std::to_string(data.getBlocksMined()));
	json.set("blocksWon", std::to_string(data.getBlocksWon()));
	json.set("onlineVersion", Settings::project.getOnlineVersion());
	json.set("runningVersion", Settings::project.getVersion());
	
	if (bestOverall != nullptr)
		json.set("bestOverall", createJsonDeadline(*bestOverall));
	else
		json.set("bestOverall", Poco::JSON::Object());

	if (bestHistorical != nullptr)
		json.set("bestHistorical", createJsonDeadline(*bestHistorical));
	else
		json.set("bestHistorical", Poco::JSON::Object());

	json.set("deadlinesConfirmed", std::to_string(data.getConfirmedDeadlines()));
	json.set("deadlinesAvg", deadlineFormat(data.getAverageDeadline()));

	//Read roundTimes and BlockTimes from blockdata
	Poco::JSON::Array roundTimeHistory;
	Poco::JSON::Array blockTimeHistory;
	auto nRTimes = 0;
	auto sumRTimes = 0.0;
	auto maxRoundTime = 0.0;
	auto nBTimes = 0;
	auto sumBTimes = 0.0;
	auto maxBlockTime = 0ull;
	const auto historicalBlockData = data.getAllHistoricalBlockData();

	for (auto& historicalRoundTime : historicalBlockData)
	{
		const auto roundTime = historicalRoundTime->getRoundTime();
		if (roundTime > 0)
		{
			Poco::JSON::Array jsonRoundTimeHistory;
			jsonRoundTimeHistory.add(std::to_string(historicalRoundTime->getBlockheight()));
			jsonRoundTimeHistory.add(std::to_string(roundTime));
			roundTimeHistory.add(jsonRoundTimeHistory);
			nRTimes++;
			sumRTimes += roundTime;
			if (roundTime > maxRoundTime) maxRoundTime = roundTime;
		}
		const auto blockTime = historicalRoundTime->getBlockTime();
		Poco::JSON::Array jsonBlockTimeHistory;
		jsonBlockTimeHistory.add(std::to_string(historicalRoundTime->getBlockheight()));
		jsonBlockTimeHistory.add(std::to_string(blockTime));
		blockTimeHistory.add(jsonBlockTimeHistory);
		nBTimes++;
		sumBTimes += blockTime;
		if (blockTime > maxBlockTime)
			maxBlockTime = blockTime;
	}
	auto meanRoundTime = 0.0;
	if (nRTimes > 0)
		meanRoundTime = sumRTimes / static_cast<double>(nRTimes);

	auto meanBlockTime = 0.0;
	if (nBTimes > 0)
		meanBlockTime = sumBTimes / static_cast<double>(nBTimes);

	json.set("meanBlockTime", std::to_string(meanBlockTime));
	json.set("maxBlockTime", std::to_string(maxBlockTime));
	json.set("blockTimeHistory", blockTimeHistory);
	json.set("meanRoundTime", std::to_string(meanRoundTime));
	json.set("maxRoundTime", std::to_string(maxRoundTime));
	json.set("roundTimeHistory", roundTimeHistory);
		
	//get deadlines from blockdata
	Poco::JSON::Array bestDeadlines;
	auto maxDeadline = 0ull;
	auto nDeadlines = 0;
	auto totalTarget = 0.0;
	auto nTargets = 0;

	for (auto& historicalDeadline : historicalBlockData)
	{
		if (historicalDeadline->getBestDeadline() != nullptr)
		{
			const auto thisDL = historicalDeadline->getBestDeadline()->getDeadline();
			Poco::JSON::Array jsonBestDeadline;
			jsonBestDeadline.add(std::to_string(historicalDeadline->getBlockheight()));
			jsonBestDeadline.add(std::to_string(thisDL));
			bestDeadlines.add(jsonBestDeadline);

			if( thisDL > maxDeadline )
				maxDeadline = thisDL;

			nDeadlines++;
			if (historicalDeadline->getBlockTime() > meanRoundTime)
			{
				totalTarget += static_cast<double>(thisDL) / (18325193796.0f / static_cast<double>(historicalDeadline->getBasetarget()));
				nTargets++;
			}
		}
	}

	json.set("nRoundsSubmitted", std::to_string(nDeadlines));

	//calc deadline performance
	if (nTargets > 0)
	{
		const auto deadlinePerformance = MinerConfig::getConfig().getDeadlinePerformanceFac() * static_cast<double>((nTargets - 1)) / totalTarget;
		json.set("deadlinePerformance", deadlinePerformance);
	}
	else {
		json.set("deadlinePerformance", 0);
	}

	//Calculate Deadline distribution from blockdata
	if (nDeadlines > 0) 
	{
		const auto nClasses = static_cast<size_t>(ceil(sqrt(nDeadlines)));
		const auto classWidth = static_cast<Poco::UInt64>(ceil(
			static_cast<double>(maxDeadline) / static_cast<double>(nClasses)) + 1);
		std::map<Poco::UInt64, Poco::UInt64> deadlineBins;

		for (auto& historicalDeadline : historicalBlockData)
		{
			if (historicalDeadline->getBestDeadline() != nullptr)
			{
				const auto thisDl = historicalDeadline->getBestDeadline()->getDeadline();
				auto bin = static_cast<Poco::UInt64>(floor(static_cast<double>(thisDl) / classWidth));

				if (bin > nClasses - 1)
					bin = nClasses - 1;

				deadlineBins[bin]++;
			}
		}

		Poco::JSON::Array deadlineDistribution;

		for (const auto& iClass : deadlineBins)
		{
			Poco::JSON::Array jsonDeadlineDistribution;
			jsonDeadlineDistribution.add(std::to_string(iClass.first * classWidth));
			jsonDeadlineDistribution.add(std::to_string(iClass.second));
			deadlineDistribution.add(jsonDeadlineDistribution);
		}

		json.set("dlDistBarWidth", std::to_string(classWidth));
		json.set("deadlineDistribution", deadlineDistribution);
	} else 
	{
		Poco::JSON::Array deadlineDistribution;	
		Poco::JSON::Array jsonDeadlineDistribution;
		jsonDeadlineDistribution.add(std::to_string(0));
		jsonDeadlineDistribution.add(std::to_string(0));
		deadlineDistribution.add(jsonDeadlineDistribution);
		json.set("deadlineDistribution", deadlineDistribution);
	}


	//Read difficulties from blockdata
	Poco::JSON::Array difficultyHistory;
	auto nDiffs = 0;
	auto sumDiffs = 0.0;

	for (auto& historicalDifficulty : historicalBlockData)
	{
		const auto blockDiff = 18325193796.0f / static_cast<float>(historicalDifficulty->getBasetarget());
		Poco::JSON::Array jsonDifficultyHistory;
		jsonDifficultyHistory.add(std::to_string(historicalDifficulty->getBlockheight()));
		jsonDifficultyHistory.add(std::to_string(blockDiff));
		difficultyHistory.add(jsonDifficultyHistory);
		nDiffs++;
		sumDiffs += blockDiff;
	}

	json.set("numHistoricals", std::to_string(nDiffs));
	json.set("meanDifficulty",std::to_string(sumDiffs/static_cast<double>(nDiffs)));
	json.set("difficultyHistory", difficultyHistory);
	json.set("bestDeadlines", bestDeadlines);
	json.set("difficulty", std::to_string(block.getDifficulty()));
	json.set("difficultyDifference", std::to_string(data.getDifficultyDifference()));

	const auto diffToJson = [&json](const HighscoreValue<Poco::UInt64>& diff, const std::string& id) {
		Poco::JSON::Object jsonDiff;
		jsonDiff.set("blockheight", std::to_string(diff.height));
		jsonDiff.set("value", std::to_string(diff.value));
		json.set(id, jsonDiff);
	};

	diffToJson(data.getLowestDifficulty(), "lowestDifficulty");
	diffToJson(data.getHighestDifficulty(), "highestDifficulty");

	return json;
}

Poco::JSON::Object Burst::createJsonConfig()
{
	Poco::JSON::Object json;
	const auto targetDeadline = MinerConfig::getConfig().getTargetDeadline();

	json.set("type", "config");
	json.set("poolUrl", MinerConfig::getConfig().getPoolUrl().getCanonical(true));
	json.set("poolUrlPort", std::to_string(MinerConfig::getConfig().getPoolUrl().getPort()));
	json.set("miningInfoUrl", MinerConfig::getConfig().getMiningInfoUrl().getCanonical(true));
	json.set("miningInfoUrlPort", std::to_string(MinerConfig::getConfig().getMiningInfoUrl().getPort()));
	json.set("walletUrl", MinerConfig::getConfig().getWalletUrl().getCanonical(true));
	json.set("walletUrlPort", std::to_string(MinerConfig::getConfig().getWalletUrl().getPort()));
	json.set("totalPlotSize", memToString(PlotSizes::getTotalBytes(PlotSizes::Type::Combined), 2));
	json.set("timeout", MinerConfig::getConfig().getTimeout());
	json.set("bufferSize", memToString(MinerConfig::getConfig().getMaxBufferSize(), 0));
	json.set("bufferSizeRaw", std::to_string(MinerConfig::getConfig().getMaxBufferSizeRaw()));
	json.set("bufferChunks", std::to_string(MinerConfig::getConfig().getBufferChunkCount()));
	json.set("targetDeadline", deadlineFormat(targetDeadline));
	json.set("submitProbability", MinerConfig::getConfig().getSubmitProbability());
	json.set("maxHistoricalBlocks", MinerConfig::getConfig().getMaxHistoricalBlocks());
	json.set("maxPlotReaders", std::to_string(MinerConfig::getConfig().getMaxPlotReaders()));
	json.set("maxPlotReadersRaw", std::to_string(MinerConfig::getConfig().getMaxPlotReaders(false)));
	json.set("miningIntensity", std::to_string(MinerConfig::getConfig().getMiningIntensity()));
	json.set("miningIntensityRaw", std::to_string(MinerConfig::getConfig().getMiningIntensity(false)));
	json.set("submissionMaxRetry", std::to_string(MinerConfig::getConfig().getSubmissionMaxRetry()));

	const auto addTargetDeadline = [&json](const std::string& id, auto value) {
		json.set("targetDeadline" + id, deadlineFormat(value));
		json.set("targetDeadline" + id + "Raw", std::to_string(value));
	};

	addTargetDeadline("Combined", MinerConfig::getConfig().getTargetDeadline());
	addTargetDeadline("Local", MinerConfig::getConfig().getTargetDeadline(TargetDeadlineType::Local));
	addTargetDeadline("Pool", MinerConfig::getConfig().getTargetDeadline(TargetDeadlineType::Pool));

	json.set("logDir", MinerConfig::getConfig().getLogDir());

	Poco::JSON::Object json_channel_priorities;

	for (auto& channel_priority : MinerLogger::getChannelPriorities())
	{
		Poco::JSON::Object json_channel_priority;
		json_channel_priority.set("numeric",
			static_cast<int>(MinerLogger::getStringToPriority(channel_priority.second)));
		json_channel_priority.set("alphaNumeric", channel_priority.second);
		json_channel_priorities.set(channel_priority.first, json_channel_priority);
	}

	json.set("channelPriorities", json_channel_priorities);

	return json;
}

Poco::JSON::Object Burst::createJsonProgress(float progressRead, float progressVerification)
{
	Poco::JSON::Object json;
	json.set("type", "progress");
	json.set("value", progressRead);
	json.set("valueVerification", progressVerification);
	return json;
}

Poco::JSON::Object Burst::createJsonLastWinner(const MinerData& data)
{
	const auto block = data.getBlockData();

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
#elif defined(_WIN32)
	const auto now = std::chrono::system_clock::now();
	auto nowC = std::chrono::system_clock::to_time_t(now);
	//ss.imbue(std::locale());
	struct tm timeinfo{};
	localtime_s(&timeinfo, &nowC);
	ss << std::put_time(&timeinfo, "%X");
#else 
	const auto now = std::chrono::system_clock::now();
	auto nowC = std::chrono::system_clock::to_time_t(now);
	//ss.imbue(std::locale());
	ss << std::put_time(std::localtime(&nowC), "%X");
#endif

	return ss.str();
}

std::string Burst::getFilenameWithtimestamp(const std::string& name, const std::string& ending)
{
	return Poco::format("%s_%s.%s",
		name, Poco::DateTimeFormatter::format(Poco::Timestamp(), "%Y%m%d_%H%M%s"), ending);
}

std::string Burst::hashHmacSha1(const std::string& plain, const std::string& passphrase)
{
	Poco::HMACEngine<Poco::SHA1Engine> engine{ passphrase };
	engine.update(plain);
	auto& digest = engine.digest();
	return Poco::DigestEngine::digestToHex(digest);
}

std::string Burst::createTruncatedString(const std::string& string, size_t padding, size_t size)
{
	std::string padded_string;

	for (size_t i = 0; i < string.size(); i += size)
	{
		const auto max_size = std::min(size, string.size());

		padded_string += string.substr(i, max_size);

		// reached the end of the string
		if (i >= string.size())
			break;
		else if (i + size < string.size())
		{
			padded_string += '\n';
			padded_string += std::string(padding, ' ');
		}
	}

	return padded_string;
}

bool Burst::cpuHasInstructionSet(CpuInstructionSet cpuInstructionSet)
{
	const auto instructionSets = cpuGetInstructionSets();

	switch (cpuInstructionSet)
	{
	case Sse2: return (instructionSets & Sse2) == Sse2;
	case Sse4: return (instructionSets & Sse4) == Sse4;
	case Avx: return (instructionSets & Avx) == Avx;
	case Avx2: return (instructionSets & Avx2) == Avx2;
	default: return false;
	}
}

int Burst::cpuGetInstructionSets()
{
#if defined __arm__
	return sse2;
#elif defined __GNUC__
	auto instruction_sets = 0;

	if (__builtin_cpu_supports("sse2"))
		instruction_sets += Sse2;

	if (__builtin_cpu_supports("sse4.2"))
		instruction_sets += Sse4;

	if (__builtin_cpu_supports("avx"))
		instruction_sets += Avx;

	if (__builtin_cpu_supports("avx2"))
		instruction_sets += Avx2;

	return instruction_sets;
#else
	int info[4];
	cpuid(info, 0);
	const auto n_ids = info[0];

	auto hasSse2 = false;
	auto hasSse4 = false;
	auto hasAvx = false;
	auto hasAvx2 = false;

	//  Detect Features
	if (n_ids >= 0x00000001)
	{
		cpuid(info, 0x00000001);
		hasSse2 = (info[3] & 1 << 26) != 0;
		hasSse4 = (info[2] & 1 << 19) != 0 || (info[2] & 1 << 20) != 0;
		hasAvx = (info[2] & 1 << 28) != 0;
	}

	if (n_ids >= 0x00000007)
	{
		cpuid(info, 0x00000007);
		hasAvx2 = (info[1] & (static_cast<int>(1) << 5)) != 0;
	}

	auto instructionSets = 0;

	if (hasSse2)
		instructionSets += Sse2;

	if (hasSse4)
		instructionSets += Sse4;

	if (hasAvx)
		instructionSets += Avx;

	if (hasAvx2)
		instructionSets += Avx2;

	return instructionSets;
#endif
}

size_t Burst::getMemorySize()
{
	/*
	 * Author:  David Robert Nadeau
	 * Site:    http://NadeauSoftware.com/
	 * License: Creative Commons Attribution 3.0 Unported License
	 *          http://creativecommons.org/licenses/by/3.0/deed.en_US
	 */


#if defined(_WIN32) && (defined(__CYGWIN__) || defined(__CYGWIN32__))
	/* Cygwin under Windows. ------------------------------------ */
	/* New 64-bit MEMORYSTATUSEX isn't available.  Use old 32.bit */
	MEMORYSTATUS status;
	status.dwLength = sizeof(status);
	GlobalMemoryStatus(&status);
	return (size_t)status.dwTotalPhys;

#elif defined(_WIN32)
	/* Windows. ------------------------------------------------- */
	/* Use new 64-bit MEMORYSTATUSEX, not old 32-bit MEMORYSTATUS */
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);
	return static_cast<size_t>(status.ullTotalPhys);

#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
	/* UNIX variants. ------------------------------------------- */
	/* Prefer sysctl() over sysconf() except sysctl() HW_REALMEM and HW_PHYSMEM */

#if defined(CTL_HW) && (defined(HW_MEMSIZE) || defined(HW_PHYSMEM64))
	int mib[2];
	mib[0] = CTL_HW;
#if defined(HW_MEMSIZE)
	mib[1] = HW_MEMSIZE;            /* OSX. --------------------- */
#elif defined(HW_PHYSMEM64)
	mib[1] = HW_PHYSMEM64;          /* NetBSD, OpenBSD. --------- */
#endif
	int64_t size = 0;               /* 64-bit */
	size_t len = sizeof(size);
	if (sysctl(mib, 2, &size, &len, NULL, 0) == 0)
		return (size_t)size;
	return 0L;			/* Failed? */

#elif defined(_SC_AIX_REALMEM)
	/* AIX. ----------------------------------------------------- */
	return (size_t)sysconf(_SC_AIX_REALMEM) * (size_t)1024L;

#elif defined(_SC_PHYS_PAGES) && defined(_SC_PAGESIZE)
	/* FreeBSD, Linux, OpenBSD, and Solaris. -------------------- */
	return (size_t)sysconf(_SC_PHYS_PAGES) *
		(size_t)sysconf(_SC_PAGESIZE);

#elif defined(_SC_PHYS_PAGES) && defined(_SC_PAGE_SIZE)
	/* Legacy. -------------------------------------------------- */
	return (size_t)sysconf(_SC_PHYS_PAGES) *
		(size_t)sysconf(_SC_PAGE_SIZE);

#elif defined(CTL_HW) && (defined(HW_PHYSMEM) || defined(HW_REALMEM))
	/* DragonFly BSD, FreeBSD, NetBSD, OpenBSD, and OSX. -------- */
	int mib[2];
	mib[0] = CTL_HW;
#if defined(HW_REALMEM)
	mib[1] = HW_REALMEM;		/* FreeBSD. ----------------- */
#elif defined(HW_PYSMEM)
	mib[1] = HW_PHYSMEM;		/* Others. ------------------ */
#endif
	unsigned int size = 0;		/* 32-bit */
	size_t len = sizeof(size);
	if (sysctl(mib, 2, &size, &len, NULL, 0) == 0)
		return (size_t)size;
	return 0L;			/* Failed? */
#endif /* sysctl and sysconf variants */

#else
	return 0L;			/* Unknown OS. */
#endif
}

// https://stackoverflow.com/questions/1413445/reading-a-password-from-stdcin
void Burst::setStdInEcho(bool enable)
{
#ifdef WIN32
	const auto hStdin = GetStdHandle(STD_INPUT_HANDLE);
	DWORD mode;
	GetConsoleMode(hStdin, &mode);

	if (!enable)
		mode &= ~ENABLE_ECHO_INPUT;
	else
		mode |= ENABLE_ECHO_INPUT;

	SetConsoleMode(hStdin, mode);

#else
	struct termios tty;
	tcgetattr(STDIN_FILENO, &tty);
	if (!enable)
		tty.c_lflag &= ~ECHO;
	else
		tty.c_lflag |= ECHO;

	(void)tcsetattr(STDIN_FILENO, TCSANOW, &tty);
#endif
}

Poco::Path Burst::getMinerHomeDir()
{
	Poco::Path minerRootPath(Poco::Path::home());
	minerRootPath.pushDirectory(".creepMiner");
	return minerRootPath.parseDirectory(minerRootPath.toString());
}

Poco::Path Burst::getMinerHomeDir(const std::string& filename)
{
	return getMinerHomeDir().append(filename);
}

std::string Burst::toHex(const std::string& plainText)
{
	std::stringstream sstream;
	Poco::HexBinaryEncoder hex{sstream};
	hex << plainText;
	hex.close();
	return sstream.str();
}

std::string Burst::fromHex(const std::string& codedText)
{
	std::stringstream sstream;
	sstream << codedText;
	Poco::HexBinaryDecoder hex{sstream};
	std::string out;
	Poco::StreamCopier::copyToString(hex, out);
	return out;
}
