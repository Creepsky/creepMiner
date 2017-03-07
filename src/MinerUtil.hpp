//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <Poco/Timespan.h>
#include <memory>
#include <Poco/Net/SocketAddress.h>
#include <array>

namespace Poco
{
	class URI;
	class TaskManager;
	namespace Net { class HTTPClientSession; }
	namespace JSON { class Object; }
}

namespace Burst
{
	class PlotReadProgress;
	class BlockData;
	class Deadline;
	class MinerData;
	
	enum class MemoryUnit : uint64_t
	{
		Megabyte = 1048576,
		Gigabyte = 1073741824,
		Terabyte = 1099511627776,
		Petabyte = 1125899906842624,
		Exabyte = 1152921504606846976
	};

	template <typename T, size_t SZ>
	std::string byteArrayToStr(const std::array<T, SZ>& arr)
	{
		std::stringstream stream;
		for (size_t i = 0; i < SZ; i++)
		{
			stream << std::setfill('0') << std::setw(sizeof(T) * 2) << std::hex << static_cast<size_t>(arr[i]);
		}
		return stream.str();
	}

	enum class PlotCheckResult
	{
		Ok,
		Error,
		EmptyParameter,
		InvalidParameter,
		WrongStaggersize,
		Incomplete
	};

	bool isNumberStr(const std::string& str);
	std::string getFileNameFromPath(const std::string& strPath);
	std::vector<std::string>& splitStr(const std::string& s, char delim, std::vector<std::string>& elems);
	std::vector<std::string> splitStr(const std::string& s, char delim);
	std::vector<std::string> splitStr(const std::string& s, const std::string& delim);
	PlotCheckResult isValidPlotFile(const std::string& filePath);
	std::string getAccountIdFromPlotFile(const std::string& path);
	std::string getStartNonceFromPlotFile(const std::string& path);
	std::string getNonceCountFromPlotFile(const std::string& path);
	std::string getStaggerSizeFromPlotFile(const std::string& path);
	std::string deadlineFormat(uint64_t seconds);
	uint64_t formatDeadline(const std::string& format);
	std::string gbToString(uint64_t size);
	std::string memToString(uint64_t size, MemoryUnit factor, uint8_t precision);
	std::string memToString(uint64_t size, uint8_t precision);
	std::string getInformationFromPlotFile(const std::string& path, uint8_t index);
	std::string encrypt(const std::string& decrypted, const std::string& algorithm, std::string& key, std::string& salt, uint32_t& iterations);
	std::string decrypt(const std::string& encrypted, const std::string& algorithm, const std::string& key, const std::string& salt, uint32_t& iterations);

	template <typename T, typename U>
	void transferSession(T& from, U& to)
	{
		auto socket = from.transferSession();

		if (socket != nullptr)
			to = std::move(socket);
	}

	Poco::Timespan secondsToTimespan(float seconds);
	std::unique_ptr<Poco::Net::HTTPClientSession> createSession(const Poco::URI& uri);
	Poco::Net::SocketAddress getHostAddress(const Poco::URI& uri);
	std::string serializeDeadline(const Deadline& deadline, std::string delimiter = ":");

	Poco::JSON::Object createJsonDeadline(const Deadline& deadline);
	Poco::JSON::Object createJsonDeadline(const Deadline& deadline, const std::string& type);
	Poco::JSON::Object createJsonNewBlock(const MinerData& data);
	Poco::JSON::Object createJsonConfig();
	Poco::JSON::Object createJsonProgress(float progress);
	Poco::JSON::Object createJsonLastWinner(const MinerData& data);
	Poco::JSON::Object createJsonShutdown();
	Poco::JSON::Object createJsonWonBlocks(const MinerData& data);

	std::string getTime();
	std::string getFilenameWithtimestamp(const std::string& name, const std::string& ending);

	std::string hash_HMAC_SHA1(const std::string& plain, const std::string& passphrase);
	bool check_HMAC_SHA1(const std::string& plain, const std::string& hashed, const std::string& passphrase);
}
