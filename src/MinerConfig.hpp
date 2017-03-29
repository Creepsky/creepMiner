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
#include <Poco/Path.h>
#include <Poco/JSON/Object.h>
#include <functional>

namespace Poco
{
	class File;
}

namespace Burst
{
	class MinerData;

	enum class HostType
	{
		Pool,
		Wallet,
		MiningInfo
	};

	class PlotFile
	{
	public:
		PlotFile(std::string&& path, uint64_t size);

		const std::string& getPath() const;
		uint64_t getSize() const;
		
	private:
		std::string path_;
		uint64_t size_;
	};

	class PlotDir
	{
	public:
		enum class Type
		{
			Sequential,
			Parallel
		};

		using PlotList = std::vector<std::shared_ptr<PlotFile>>;

		PlotDir(std::string path, Type type);
		PlotDir(std::string path, const std::vector<std::string>& relatedPaths, Type type);

		const PlotList& getPlotfiles() const;
		const std::string& getPath() const;
		uint64_t getSize() const;
		Type getType() const;
		std::vector<std::shared_ptr<PlotDir>> getRelatedDirs() const;
		void rescan();

	private:
		bool addPlotLocation(const std::string& fileOrPath);
		std::shared_ptr<PlotFile> addPlotFile(const Poco::File& file);

		std::string path_;
		Type type_;
		uint64_t size_;
		PlotList plotfiles_;
		std::vector<std::shared_ptr<PlotDir>> relatedDirs_;
	};

	class Socket;

	class MinerConfig
	{
	public:
		bool readConfigFile(const std::string& configPath);
		void rescan();
		void rescanPlotfiles();
		void printConsole() const;
		void printConsolePlots() const;

		size_t maxBufferSizeMB = 128;
		const std::string& getPath() const;

		std::vector<std::shared_ptr<PlotFile>> getPlotFiles() const;
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
		bool forPlotDirs(std::function<bool(PlotDir&)> traverseFunction) const;
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
		static Poco::JSON::Object::Ptr readOutput(Poco::JSON::Object::Ptr json);
		
		std::string configPath_;
		std::vector<std::shared_ptr<PlotDir>> plotDirs_;
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
		Url serverUrl_;
		uint64_t targetDeadline_ = 0;
		uint32_t miningIntensity_ = 1;
		std::string plotsHash_;
		std::string passPhrase_;
		std::string serverUser_, serverPass_;
		uint32_t maxPlotReaders_ = 0;
		Poco::Path pathLogfile_ = "";
		mutable Poco::FastMutex mutexPlotfiles_;
	};
}
