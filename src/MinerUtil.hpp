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

#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <Poco/Timespan.h>
#include <memory>
#include <Poco/Net/SocketAddress.h>
#include <array>
#include <Poco/JSON/Array.h>
#include <Poco/Path.h>

namespace Poco
{
	class URI;
	class TaskManager;
	namespace Net { class HTTPClientSession; }
	namespace JSON { class Object; }
}

namespace Burst
{
	class PlotDir;
	class PlotReadProgress;
	class BlockData;
	class Deadline;
	class MinerData;
	
	enum class MemoryUnit : Poco::UInt64
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

	enum class DeadlineFragment
	{
		Years,
		Months,
		Days,
		Hours,
		Minutes,
		Seconds
	};

	enum CpuInstructionSet
	{
		Sse2 = 1 << 0,
		Sse4 = 1 << 1,
		Avx = 1 << 2,
		Avx2 = 1 << 3
	};

	bool isNumberStr(const std::string& str);
	std::vector<std::string>& splitStr(const std::string& s, char delim, std::vector<std::string>& elems);
	std::vector<std::string> splitStr(const std::string& s, char delim);
	std::vector<std::string> splitStr(const std::string& s, const std::string& delim);
	PlotCheckResult isValidPlotFile(const std::string& filePath);
	std::string getAccountIdFromPlotFile(const std::string& path);
	std::string getStartNonceFromPlotFile(const std::string& path);
	std::string getNonceCountFromPlotFile(const std::string& path);
	std::string getStaggerSizeFromPlotFile(const std::string& path);
	std::string getVersionFromPlotFile(const std::string& path);
	std::string deadlineFormat(Poco::UInt64 seconds);
	Poco::UInt64 deadlineFragment(Poco::UInt64 seconds, DeadlineFragment fragment);
	Poco::UInt64 formatDeadline(const std::string& format);
	std::string gbToString(Poco::UInt64 size);
	std::string memToString(Poco::UInt64 size, MemoryUnit factor, Poco::UInt8 precision);
	std::string memToString(Poco::UInt64 size, Poco::UInt8 precision);
	std::string getInformationFromPlotFile(const std::string& path, Poco::UInt8 index);
	std::string encrypt(const std::string& decrypted, const std::string& algorithm, std::string& key, std::string& salt, Poco::UInt32 iterations);
	std::string decrypt(const std::string& encrypted, const std::string& algorithm, const std::string& key, const std::string& salt, Poco::UInt32 iterations);

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
	std::string serializeDeadline(const Deadline& deadline, const std::string& delimiter = ":");

	Poco::JSON::Object createJsonDeadline(const Deadline& deadline);
	Poco::JSON::Object createJsonDeadline(const Deadline& deadline, const std::string& type);
	Poco::JSON::Object createJsonNewBlock(const MinerData& data);
	Poco::JSON::Object createJsonConfig();
	Poco::JSON::Object createJsonProgress(float progressRead, float progressVerification);
	Poco::JSON::Object createJsonLastWinner(const MinerData& data);
	Poco::JSON::Object createJsonShutdown();
	Poco::JSON::Object createJsonWonBlocks(const MinerData& data);
	Poco::JSON::Object createJsonPlotDir(const PlotDir& plotDir);
	Poco::JSON::Array createJsonPlotDirs();
	Poco::JSON::Object createJsonPlotDirsRescan();

	std::string getTime();
	std::string getFilenameWithtimestamp(const std::string& name, const std::string& ending);

	std::string hashHmacSha1(const std::string& plain, const std::string& passphrase);

	/*
	 * \brief Creates a string that is padded and locked on a specific size.
	 * \param string The string, that is sized.
	 * \param padding The padding from left side of the string.
	 * \param size The max. size of the string. Every time the string exceeds the size,
	 * it gets wrapped into a new line.
	 */
	std::string createTruncatedString(const std::string& string, size_t padding, size_t size);

	template <typename T>
	std::string numberToString(T number)
	{
		std::stringstream sstream;
		sstream.imbue(std::locale(""));

		sstream << number;
		return sstream.str();
	}

	bool cpuHasInstructionSet(CpuInstructionSet cpuInstructionSet);
	int cpuGetInstructionSets();

	size_t getMemorySize();
	void setStdInEcho(bool enable);

	Poco::Path getMinerHomeDir();
	Poco::Path getMinerHomeDir(const std::string& filename);

	std::string toHex(const std::string& plainText);
	std::string fromHex(const std::string& codedText);

	std::string createBuildFeatures();

	std::string jsonToString(const Poco::JSON::Object& json);

	class LowLevelFileStream
	{
	public:
		LowLevelFileStream(const LowLevelFileStream& other);
		LowLevelFileStream(LowLevelFileStream&& other) noexcept;
		LowLevelFileStream(const std::string& path);
		LowLevelFileStream& operator=(const LowLevelFileStream& other);
		LowLevelFileStream& operator=(LowLevelFileStream&& other) noexcept;
		~LowLevelFileStream();

		bool seekg(size_t offset) const;
		bool read(char* buffer, size_t bytes) const;
		operator bool() const;

	private:
#ifdef _WIN32
		void* handle_;
#else
		int handle_;
#endif
	};
}
