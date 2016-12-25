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
#include <string>
#include <Poco/Net/HTTPClientSession.h>
#include "Url.hpp"

/*
 {
	poolUrl : "burst-pool.cryptoport.io:80",
	submissionMaxDelay : 60000,
	submissionMaxRetry : 3,
	plots : [
		"/mnt/sda/plots/",
		"/mnt/sdb/plots/"
	]
 }
 */

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

	struct Output
	{
		bool progress = false;
		bool debug = false;
		bool nonceFound = true;
		bool nonceFoundPlot = false;
		bool nonceConfirmedPlot = false;
		bool plotDone = false;
		bool dirDone = false;
	};

	class Socket;

	class MinerConfig
	{
	public:
		bool readConfigFile(const std::string& configPath);
		void rescan();

		size_t maxBufferSizeMB = 128;
		Output output;
		const std::string& getPath() const;

		const std::vector<std::shared_ptr<PlotFile>>& getPlotFiles() const;
		uintmax_t getTotalPlotsize() const;

		float getReceiveTimeout() const;
		float getSendTimeout() const;
		const Url& getPoolUrl() const;
		const Url& getMiningInfoUrl() const;
		const Url& getWalletUrl() const;

		size_t getReceiveMaxRetry() const;
		size_t getSendMaxRetry() const;
		size_t getSubmissionMaxRetry() const;
		size_t getHttp() const;
		const std::string& getConfirmedDeadlinesPath() const;

		std::unique_ptr<Socket> createSocket(HostType hostType) const;
		std::unique_ptr<Poco::Net::HTTPClientSession> createSession(HostType hostType) const;
		std::unique_ptr<Poco::Net::HTTPClientSession> createSession(const std::string& url) const;

		static MinerConfig& getConfig();

	private:
		bool addPlotLocation(const std::string& fileOrPath);
		std::shared_ptr<PlotFile> addPlotFile(const std::string& file);

		std::string configPath_;
		std::vector<std::shared_ptr<PlotFile>> plotList_;
		float receive_timeout_ = 3.f;
		float send_timeout_ = 3.f;
		size_t send_max_retry_ = 3;
		size_t receive_max_retry_ = 3;
		size_t submission_max_retry_ = 3;
		size_t http_ = 0;
		std::string confirmedDeadlinesPath_ = "";
		Url urlPool_;
		Url urlMiningInfo_;
		Url urlWallet_;
	};
}
