//  cryptoport.io Burst Pool Miner
//
//  Created by Uray Meiviar < uraymeiviar@gmail.com > 2014
//  donation :
//
//  [Burst  ] BURST-8E8K-WQ2F-ZDZ5-FQWHX
//  [Bitcoin] 1UrayjqRjSJjuouhJnkczy5AuMqJGRK4b

#pragma once

#include <memory>
#include <vector>
#include <Poco/Net/HTTPClientSession.h>
#include "Url.hpp"
#include <unordered_map>
#include <Poco/Path.h>

namespace Poco
{
	class File;
}

namespace Burst
{
	enum class HostType
	{
		Pool,
		Wallet,
		MiningInfo
	};

	class PlotFile
	{
	public:
		PlotFile(std::string&& path, size_t size);

		const std::string& getPath() const;
		size_t getSize() const;

	private:
		std::string path;
		size_t size;
	};

	class Socket;

	class MinerConfig
	{
	public:
		using PlotList = std::vector<std::shared_ptr<PlotFile>>;

		bool readConfigFile(const std::string& configPath);
		void rescan();

		size_t maxBufferSizeMB = 128;
		const std::string& getPath() const;

		const std::vector<std::shared_ptr<PlotFile>>& getPlotFiles() const;
		uintmax_t getTotalPlotsize() const;

		float getReceiveTimeout() const;
		float getSendTimeout() const;
		float getTimeout() const;
		const Url& getPoolUrl() const;
		const Url& getMiningInfoUrl() const;
		const Url& getWalletUrl() const;

		size_t getReceiveMaxRetry() const;
		size_t getSendMaxRetry() const;
		size_t getSubmissionMaxRetry() const;
		size_t getHttp() const;
		const std::string& getConfirmedDeadlinesPath() const;
		bool getStartServer() const;
		const Url& getServerUrl() const;
		uint64_t getTargetDeadline() const;
		uint32_t getMiningIntensity() const;
		const std::unordered_map<std::string, PlotList>& getPlotList() const;
		const std::string& getPlotsHash() const;
		const std::string& getPassphrase() const;
		uint32_t getMaxPlotReaders() const;
		const Poco::Path& getPathLogfile() const;
		const std::string& getServerUser() const;
		const std::string& getServerPass() const;

		std::unique_ptr<Socket> createSocket(HostType hostType) const;
		std::unique_ptr<Poco::Net::HTTPClientSession> createSession(HostType hostType) const;

		static const std::string WebserverPassphrase;

		static MinerConfig& getConfig();
		
	private:
		bool addPlotLocation(const std::string& fileOrPath);
		std::shared_ptr<PlotFile> addPlotFile(const Poco::File& file);

		std::string configPath_;
		std::vector<std::shared_ptr<PlotFile>> plotList_;
		float timeout_ = 30.f;
		size_t send_max_retry_ = 3;
		size_t receive_max_retry_ = 3;
		size_t submission_max_retry_ = 3;
		size_t http_ = 0;
		std::string confirmedDeadlinesPath_ = "";
		Url urlPool_;
		Url urlMiningInfo_;
		Url urlWallet_;
		bool startServer_ = false;
		Url serverUrl_ = {"127.0.0.1:9999"};
		uint64_t targetDeadline_ = 0;
		uint32_t miningIntensity_ = 1;
		std::unordered_map<std::string, PlotList> plotDirs_;
		std::string plotsHash_;
		std::string passPhrase_;
		std::string serverUser_, serverPass_;
		uint32_t maxPlotReaders_ = 0;
		Poco::Path pathLogfile_ = "";
	};
}
